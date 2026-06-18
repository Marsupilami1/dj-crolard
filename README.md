# DJ Crolard

A collaborative YouTube DJ application that allows multiple users to synchronize and control video playback in real-time.

Uses YouTube API for video metadata (API key required).

## Prerequisites

- **C++23 compatible compiler** (clang++ recommended)
- **Dependencies:**
  - [Crow](https://github.com/CrowCpp/Crow) - C++ web framework (included, header-only)
  - [cpr](https://github.com/libcpr/cpr) - C++ Requests library
  - [asio](https://github.com/chriskohlhoff/asio) - Asynchronous I/O

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential clang libcurl4-openssl-dev libasio-dev libcpr-dev
```

### Fedora

```bash
sudo dnf install gcc-c++ clang libcurl-devel asio-devel cpr-devel
```

### Arch Linux

```bash
sudo pacman -S base-devel clang curl asio

# cpr is available in the AUR (use your preferred AUR helper)
yay -S cpr
# or
paru -S cpr
```

## YouTube API Key Setup

### Option 1: Environment Variable

Set the API key directly as an environment variable:

```bash
export YT_API_KEY="your-youtube-api-key-here"
```

### Option 2: API Key File

Create a file containing your API key:

```bash
# Default location (api-key.txt in the server directory)
echo "your-youtube-api-key-here" > server/api-key.txt

# Or specify a custom path
export YT_API_KEY_FILE="/path/to/your/api-key-file"
```

## Building and running

From the **root** directory:

```bash
make -C server all # Build
./server/main      # Run
```

The server will start on port 8000. Open `http://localhost:8000` in your browser.

## Development

### Project Structure

```
.
├── README.md           # This file
├── public/             # Frontend static files
└── server/             # Backend C++ server
```
