let ws = new WebSocket("/ws");
let player;
let currentVideoId = null;
let currentVideoStartTime = null;
let sortableInstance = null;
let localQueue = [];
let viewers_count = null;

// DOM elements
let input = null;
let dropdown = null;
let search_results = null;
let viewers = null;

window.onload = function () {
	input = document.getElementById("search-bar");
	input.addEventListener("keydown", (e) => {
		if (e.key === "Enter") search();
	});

	dropdown = document.getElementById("dropdown");
	// close dropdown if a click occurs somewhere else on the page
	document.addEventListener("click", (e) => {
		if (!e.target.closest("#search-wrap")) closeDropdown();
	});

	search_results = document.getElementById("search-results");

	viewers = document.getElementById("viewers");
	renderViewers();
};

function openDropdown() {
	search_results.innerHTML = `<div class="flex justify-center gap-2">
			<svg
				class="mr-3 -ml-1 size-5 animate-spin text-white"
				xmlns="http://www.w3.org/2000/svg"
				fill="none"
				viewBox="0 0 24 24"
			>
				<circle
					class="opacity-25"
					cx="12"
					cy="12"
					r="10"
					stroke="currentColor"
					stroke-width="4"
				></circle>
				<path
					class="opacity-75"
					fill="currentColor"
					d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
				></path>
			</svg>
		</div>`;
	dropdown.classList.add("open");
}

function closeDropdown() {
	dropdown.classList.remove("open");
}

function isDropdownOpen() {
	return dropdown.classList.contains("open");
}

function renderViewers() {
	viewers.innerHTML = `
			<div class="flex flex-col items-center">
				<div
					class="h-15 w-15 rounded-full border-2 border-mist-900 bg-mist-600 shadow-lg shadow-mist-900"
				></div>
				<div
					class="flex h-20 w-25 rounded-t-full border-2 border-b-0 border-mist-900 bg-mist-600 shadow-mist-900 shadow-lg items-center justify-center"
				>
					<span class="font-bold"></span>
				</div>
			</div>`.repeat(viewers_count);
}

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

function sendId(id) {
	req = { message: "add", payload: id };
	ws.send(JSON.stringify(req));
}

function search() {
	const id = extractId(input.value);
	if (id) sendId(id);
	else {
		req = { message: "search", payload: input.value };
		ws.send(JSON.stringify(req));
		openDropdown();
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
		currentVideoStartTime = Date.now() - payload.elapsedTime;
		forceSync();
		return;
	}
	if (data.message == "viewers") {
		console.log("Updating viewers");
		viewers_count = payload;
		if (viewers) renderViewers();
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
	if (data.message == "search-response") {
		console.log(data.payload);
		if (payload.length == 0) {
			search_results.innerHTML = "<div> No result found :/</div>";
		} else {
			search_results.innerHTML = payload
				.map(
					(item, i) =>
						` <div class="flex gap-6 items-center p-2 hover:bg-mist-800 hover:cursor-pointer group"
										onclick="sendId('${item.id}')">
							<img
								src="${item.thumbnail}"
								class="h-[90px] rounded-md border-zinc-200 border-1"
							/>
							<div class="flex flex-1 text-zinc-100">
								${item.title}
							</div>
							<i
								class="hidden group-hover:inline bi bi-plus text-4xl"
							></i></div> `,
				)
				.join('<div class="flex border-b-1 border-zinc-500 my-2"></div>');
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
		const elapsed = Date.now() - currentVideoStartTime;
		player.loadVideoById({
			videoId: currentVideoId,
			startSeconds: elapsed / 1000,
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
