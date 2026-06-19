let ws = new WebSocket("/ws");
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
		req = { message: "add", payload: id };
		ws.send(JSON.stringify(req));
	}
}

// WebSocket Events
ws.onmessage = (event) => {
	console.log("Message received from server");
	data = JSON.parse(event.data);
	payload = data.payload;
	if (data.message == "sync") {
		console.log("Synchronizing");
		currentVideoId = payload.videoId;
		currentVideoStartTime = Date.now() + payload.elapsedTime;
		forceSync();
		return;
	}
	if (data.message == "queue") {
		console.log("Updating queue");
		localQueue = payload;
		const view = document.getElementById("playlist-view");
		if (payload.length == 0) {
			view.innerHTML = "<li>Queue is empty :/</li>";
		} else {
			view.innerHTML = payload
				.map(
					(item, i) =>
						`<li data-index="${i}">
            <div class="item-content">
              <span class="index">${i + 1}</span> ${item.title}
            </div>
            <button class="delete-btn" onclick="askDelete(${i})"><i class="bi bi-trash text-xl"></i></button>
          </li>`,
				)
				.join("");
			initSortable();
		}
	}
};

function initSortable() {
	const el = document.getElementById("playlist-view");
	if (sortableInstance) sortableInstance.destroy();

	sortableInstacne = new Sortable(el, {
		animation: 150,
		ghostClass: "sortable-ghost",
		onEnd: function () {
			const items = el.querySelectorAll("li");
			const newQueue = Array.from(items).map((li) =>
				parseInt(li.dataset.index),
			);
			ws.send(JSON.stringify({ message: "reorder_queue", payload: newQueue }));
		},
	});
}

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
	req = { message: "next", payload: null };
	ws.send(JSON.stringify(req));
}

function askClearQueue() {
	req = { message: "clear", payload: null };
	ws.send(JSON.stringify(req));
}

function askDelete(index) {
	req = { message: "delete", payload: index };
	ws.send(JSON.stringify(req));
}
