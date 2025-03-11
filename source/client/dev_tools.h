#pragma once

using DevToolCleanupCallback = void ( * )();
using DevToolRenderCallback = DevToolCleanupCallback ( * )();

DevToolCleanupCallback DrawModelViewer();
