const socket = io();
let player;
let currentVideoId = null;
let currentVideoStartTime = null;
let sortableInstance = null;
let localQueue = [];

// Initialisation API YouTube
function onYouTubeIframeAPIReady() {
	player = new YT.Player("player", {
		height: "100%",
		width: "100%",
		playerVars: { autoplay: 1, controls: 1 },
		events: {
			onReady: () => {
				document.getElementById("btn-sync").style.display = "block";
			},
			onStateChange: (e) => {
				if (e.data === YT.PlayerState.PAUSED)
					console.log("Pause detected. Clic Synchronize to come back to live.");
			},
		},
	});
}

// Extraction ID
function extractId(url) {
	const reg =
		/(?:youtube\.com\/(?:[^\/]+\/.+\/|(?:v|e(?:mbed)?)\/|.*[?&]v=)|youtu\.be\/)([^"&?\/\s]{11})/i;
	const match = url.match(reg);
	return match ? match[1] : null;
}

function sendUrl() {
	const input = document.getElementById("url-input");
	const id = extractId(input.value);
	if (id) {
		socket.emit("add_to_queue", id);
		input.value = "";
	}
}

// WebSocket Events
socket.on("sync", (data) => {
	currentVideoId = data.videoId;
	currentVideoStartTime = data.startTime;
	if (player && player.loadVideoById) forceSync();
});

socket.on("update_queue", (queue) => {
	localQueue = queue;
	const view = document.getElementById("playlist-view");
	if (queue.length == 0) {
		view.innerHTML = "<li>Queue is empty :/</li>";
	} else {
		view.innerHTML = queue
			.map(
				(item, i) =>
					`<li data-index="${i}">
             <div class="item-content">
               <span class="index">${i + 1}</span> ${item.title}
             </div>
             <button class="delete-btn" onclick="askDelete('${item.id}', ${i})"><i class="bi bi-trash text-xl"></i></button>
           </li>`,
			)
			.join("");
		initSortable();
	}
});

function initSortable() {
	const el = document.getElementById("playlist-view");
	if (sortableInstance) sortableInstance.destroy();

	sortableInstacne = new Sortable(el, {
		animation: 150,
		ghostClass: "sortable-ghost",
		onEnd: function () {
			const items = el.querySelectorAll("li");
			const newQueue = Array.from(items).map(
				(li) => localQueue[li.dataset.index],
			);
			socket.emit("reorder", newQueue);
		},
	});
}

socket.on("stop_video", () => {
	if (player) player.stopVideo();
});

// La fonction clé : calcule le temps réel par rapport au serveur
function forceSync() {
	if (player && currentVideoId && currentVideoStartTime) {
		const elapsed = (Date.now() - currentVideoStartTime) / 1000;
		player.loadVideoById({
			videoId: currentVideoId,
			startSeconds: elapsed,
		});
		player.playVideo();
	}
}

function askNextVideo() {
	socket.emit("next");
}

function askClearQueue() {
	socket.emit("clear");
}

function askDelete(videoId, index) {
	socket.emit("delete", videoId, index);
}
