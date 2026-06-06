#pragma once
#ifdef __EMSCRIPTEN__

#include <string>

// == Persistent storage via IndexedDB ========================================
// Call WebCache_Mount() once at startup, then WebCache_Load() to populate the
// virtual FS from IndexedDB.  After extraction, call WebCache_Save() to
// persist new/updated files back to IndexedDB.

// Mount IDBFS at the given virtual path.  Call before any FS operations that
// need persistence.  The path is created if it does not exist.
void WebCache_Mount(const char* path);

// Populate the virtual FS from IndexedDB (blocking via ASYNCIFY).
void WebCache_Load();

// Flush the virtual FS back to IndexedDB (blocking via ASYNCIFY).
void WebCache_Save();

// == Browser file picker ======================================================
// Shows the native browser file-open dialog filtered to N64 ROM extensions.
// Blocks (via ASYNCIFY) until the user picks a file or dismisses the dialog.
// On success the chosen file is written to /tmp/rom.z64 and that path is
// returned.  Returns an empty string if the user cancels.
std::string WebFilePicker_PickROM();

#endif // __EMSCRIPTEN__
