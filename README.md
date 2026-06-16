# DJ Crolard

A collaborative YouTube DJ application that allows multiple users to synchronize and control video playback in real-time.

Uses YouTube API for video metadata (API key required).

## Prerequisites

- **C++23 compatible compiler** (clang++ recommended)
- **Dependencies:**
  - [Crow](https://github.com/CrowCpp/Crow) - C++ web framework (included, header-only)
  - [cpr](https://github.com/libcpr/cpr) - C++ Requests library
  - [asio](https://github.com/chriskohlhoff/asio) - Asynchronous I/O

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
cd server        # Go to server directory
make all         # Build
cd ..            # Go back to the root directory
./server/main    # Run
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
