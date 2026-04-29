(function (root, factory) {
  var api = factory(root);

  if (typeof module === "object" && module.exports) {
    module.exports = api;
  }

  root.Multigauge = api;
})(typeof globalThis !== "undefined" ? globalThis : window, function (root) {
  var moduleRef = null;
  var initPromise = null;
  var shutdownFlag = false;
  var nextCanvasId = 0;
  var runtimeStates = new Set();
  var editorStates = new Set();
  var wasmScriptUrl = resolveWasmScriptUrl();

  function resolveWasmScriptUrl() {
    if (root.document && root.document.currentScript && root.document.currentScript.src) {
      return new URL("wasm/multigauge.js", root.document.currentScript.src).href;
    }
    if (root.location && root.location.href) {
      return new URL("wasm/multigauge.js", root.location.href).href;
    }
    return "wasm/multigauge.js";
  }

  function userError(message) {
    return new Error(message);
  }

  function restoreGlobalModule(previousModule, internalModule) {
    if (root.Module !== internalModule) {
      return;
    }

    if (typeof previousModule === "undefined") {
      try {
        delete root.Module;
      } catch (_) {
        root.Module = undefined;
      }
    } else {
      root.Module = previousModule;
    }
  }

  function ensureReady() {
    if (shutdownFlag) {
      throw userError("Multigauge has been shut down. Reload the page to use it again.");
    }
    if (!moduleRef) {
      throw userError("Multigauge is not ready yet. Call await Multigauge.init().");
    }
  }

  function call(symbol, returnType, argTypes, args) {
    ensureReady();

    if (typeof moduleRef.ccall !== "function") {
      throw userError("Multigauge is not ready yet. Call await Multigauge.init().");
    }

    return moduleRef.ccall(symbol, returnType, argTypes, args);
  }

  function callQuiet(symbol, returnType, argTypes, args) {
    if (shutdownFlag || !moduleRef || typeof moduleRef.ccall !== "function") {
      return null;
    }

    try {
      return moduleRef.ccall(symbol, returnType, argTypes, args);
    } catch (_) {
      return null;
    }
  }

  function toJson(value) {
    return typeof value === "string" ? value : JSON.stringify(value);
  }

  function parseResult(label, jsonText) {
    var result = JSON.parse(jsonText);
    if (!result.ok) {
      throw userError(label + " failed: " + (result.error || "unknown error"));
    }
    return result.data;
  }

  function unwrapHandle(handle) {
    if (handle && typeof handle === "object" && typeof handle.id === "number") {
      return handle.id;
    }
    if (typeof handle === "number") {
      return handle;
    }
    throw userError("Expected a handle object or numeric id.");
  }

  function makeHandle(id, kind) {
    return Object.freeze({ id: id, kind: kind || "handle" });
  }

  function ensureCanvasId(canvas) {
    if (!canvas.id) {
      nextCanvasId += 1;
      canvas.id = "mg-canvas-" + nextCanvasId;
    }
    return canvas.id;
  }

  function syncCanvasBackingSize(canvas) {
    if (!canvas || typeof root.devicePixelRatio === "undefined") {
      return;
    }

    var dpr = root.devicePixelRatio || 1;
    var displayWidth = canvas.clientWidth || canvas.width || 1;
    var displayHeight = canvas.clientHeight || canvas.height || 1;
    var pixelWidth = Math.max(1, Math.round(displayWidth * dpr));
    var pixelHeight = Math.max(1, Math.round(displayHeight * dpr));

    if (canvas.width !== pixelWidth) {
      canvas.width = pixelWidth;
    }

    if (canvas.height !== pixelHeight) {
      canvas.height = pixelHeight;
    }
  }

  function markDestroyedState(state) {
    state.destroyed = true;
    state.started = false;
    state.frameActive = false;
    state.loadedGauge = null;
    state.editorId = 0;
    state.faceId = 0;
    state.contextId = 0;
  }

  function detachRuntimeFromEditor(runtimeState, clearScreen) {
    if (!runtimeState.editorState) {
      runtimeState.editorId = 0;
      runtimeState.faceId = 0;
      runtimeState.loadedGauge = null;
      return;
    }

    runtimeState.editorState.boundRuntimes.delete(runtimeState);
    runtimeState.editorState = null;
    runtimeState.editorId = 0;
    runtimeState.faceId = 0;

    if (clearScreen && runtimeState.contextId) {
      callQuiet("mg_runtime_clear_screen", "number", ["number"], [runtimeState.contextId]);
    }
  }

  function detachRuntimeFromEditorForDestroy(runtimeState, fromShutdown) {
    if (!runtimeState.editorState) {
      runtimeState.editorId = 0;
      runtimeState.faceId = 0;
      runtimeState.loadedGauge = null;
      return;
    }

    runtimeState.editorState.boundRuntimes.delete(runtimeState);
    runtimeState.editorState = null;
    runtimeState.editorId = 0;
    runtimeState.faceId = 0;

    if (fromShutdown && runtimeState.contextId) {
      callQuiet("mg_runtime_clear_screen", "number", ["number"], [runtimeState.contextId]);
    }
  }

  function ensureRuntimeAlive(state) {
    if (shutdownFlag) {
      throw userError("Multigauge has been shut down. Reload the page to use it again.");
    }
    if (state.destroyed) {
      throw userError("This runtime has been destroyed.");
    }
  }

  function ensureEditorAlive(state) {
    if (shutdownFlag) {
      throw userError("Multigauge has been shut down. Reload the page to use it again.");
    }
    if (state.destroyed) {
      throw userError("This editor has been destroyed.");
    }
  }

  function init() {
    if (shutdownFlag) {
      return Promise.reject(userError("Multigauge has been shut down. Reload the page to use it again."));
    }
    if (moduleRef) {
      return Promise.resolve(api);
    }
    if (initPromise) {
      return initPromise;
    }
    if (typeof document === "undefined") {
      return Promise.reject(userError("Multigauge.init() requires a browser."));
    }

    initPromise = new Promise(function(resolve, reject) {
      var previousModule = root.Module;
      var internalModule = {
        noInitialRun: true,
        locateFile: function(path) {
          return new URL(path, wasmScriptUrl).href;
        },
        onRuntimeInitialized: function() {
          try {
            moduleRef = internalModule;
            if (typeof internalModule.callMain !== "function") {
              throw userError("Multigauge failed to initialize its runtime.");
            }
            internalModule.callMain([]);
            resolve(api);
          } catch (err) {
            moduleRef = null;
            reject(userError("Multigauge failed to initialize: " + (err && err.message ? err.message : String(err))));
          } finally {
            restoreGlobalModule(previousModule, internalModule);
          }
        }
      };

      root.Module = internalModule;

      var script = document.createElement("script");
      script.src = wasmScriptUrl;
      script.async = true;
      script.onerror = function() {
        moduleRef = null;
        initPromise = null;
        restoreGlobalModule(previousModule, internalModule);
        reject(userError("Multigauge could not load its WebAssembly module."));
      };

      document.head.appendChild(script);
    }).then(function(result) {
      initPromise = null;
      return result;
    }, function(err) {
      initPromise = null;
      throw err;
    });

    return initPromise;
  }

  function createRuntime(canvas) {
    if (!canvas) {
      throw userError("createRuntime(canvas) requires a canvas element.");
    }

    ensureReady();
    ensureCanvasId(canvas);

    var state = {
      canvas: canvas,
      contextId: 0,
      loadedGauge: null,
      editorId: 0,
      faceId: 0,
      editorState: null,
      started: false,
      destroyed: false,
      frameActive: false,
      frameHandle: 0
    };

    runtimeStates.add(state);

    function stopLoop() {
      state.frameActive = false;
      if (state.frameHandle && typeof root.cancelAnimationFrame === "function") {
        root.cancelAnimationFrame(state.frameHandle);
      }
      state.frameHandle = 0;
    }

    function createContext() {
      if (state.contextId) {
        return state.contextId;
      }

      var contextId = call("mg_runtime_create_context", "number", ["string"], [state.canvas.id]);
      if (contextId === 0 || contextId === -1 || contextId === 0xffffffff) {
        throw userError("Could not create a rendering context.");
      }

      state.contextId = contextId;
      return contextId;
    }

    function applyBinding() {
      if (!state.contextId) {
        throw userError("The runtime context is not available yet.");
      }

      if (state.editorState && state.faceId) {
        var editorOk = call("mg_runtime_set_editor_screen", "number", ["number", "number", "number"], [
          state.contextId,
          state.editorId,
          state.faceId
        ]);
        if (!editorOk) {
          throw userError("Could not bind the editor to the runtime.");
        }
        return;
      }

      if (state.loadedGauge !== null) {
        var gaugeOk = call("mg_runtime_set_gauge_screen", "number", ["number", "string"], [
          state.contextId,
          toJson(state.loadedGauge)
        ]);
        if (!gaugeOk) {
          throw userError("Could not load the gauge into the runtime.");
        }
        return;
      }

      throw userError("Call runtime.load(json) or runtime.bindEditor(editor, faceId) before start().");
    }

    function startLoop() {
      if (state.frameActive) {
        return;
      }

      state.frameActive = true;

      function tick() {
        if (!state.frameActive || state.destroyed || shutdownFlag) {
          return;
        }

        syncCanvasBackingSize(state.canvas);
        call("mg_runtime_frame", "void", [], []);
        state.frameHandle = root.requestAnimationFrame(tick);
      }

      state.frameHandle = root.requestAnimationFrame(tick);
    }

    function destroyInternal(fromShutdown) {
      if (state.destroyed) {
        if (fromShutdown) {
          return false;
        }
        throw userError("This runtime has already been destroyed.");
      }

      stopLoop();
      detachRuntimeFromEditorForDestroy(state, fromShutdown);

      if (state.contextId) {
        var removed = callQuiet("mg_runtime_remove_context", "number", ["number"], [state.contextId]);
        if (!fromShutdown && !removed) {
          throw userError("Could not release the rendering context.");
        }
      }

      runtimeStates.delete(state);
      markDestroyedState(state);
      state.canvas = null;
      state.frameHandle = 0;
      return true;
    }

    var runtime = {
      load: function(json) {
        ensureRuntimeAlive(state);

        if (state.editorState) {
          detachRuntimeFromEditor(state, true);
        }

        state.loadedGauge = json;
        if (state.started) {
          applyBinding();
        }
        return runtime;
      },

      bindEditor: function(editor, faceId) {
        ensureRuntimeAlive(state);

        var editorState = editor && editor.__mgState ? editor.__mgState : null;
        var editorId = editorState ? editorState.editorId : unwrapHandle(editor);
        var faceHandle = unwrapHandle(faceId);

        var editorExists = call("mg_editor_exists", "number", ["number"], [editorId]);
        if (!editorExists) {
          throw userError("The editor is no longer available.");
        }

        var faceOk = call("mg_editor_is_face", "number", ["number", "number"], [editorId, faceHandle]);
        if (!faceOk) {
          throw userError("The selected face does not belong to that editor.");
        }

        if (state.editorState && state.editorState !== editorState) {
          state.editorState.boundRuntimes.delete(state);
        }

        state.loadedGauge = null;
        state.editorState = editorState;
        state.editorId = editorId;
        state.faceId = faceHandle;

        if (editorState) {
          editorState.boundRuntimes.add(state);
        }

        if (state.started) {
          applyBinding();
        }
        return runtime;
      },

      start: function() {
        ensureRuntimeAlive(state);

        if (state.started) {
          return runtime;
        }

        syncCanvasBackingSize(state.canvas);
        createContext();
        applyBinding();
        state.started = true;
        startLoop();
        return runtime;
      },

      destroy: function() {
        return destroyInternal(false);
      },

      _detachEditor: function(editorState) {
        ensureRuntimeAlive(state);

        if (state.editorState !== editorState) {
          return;
        }

        detachRuntimeFromEditor(state, true);
      },

      get canvas() {
        return state.canvas;
      },

      get contextId() {
        return state.contextId;
      }
    };

    Object.defineProperty(runtime, "__mgState", {
      value: state,
      enumerable: false
    });

    return runtime;
  }

  function createEditor() {
    ensureReady();

    var editorId = call("mg_editor_create", "number", [], []);
    if (editorId === 0 || editorId === -1 || editorId === 0xffffffff) {
      throw userError("Could not create an editor session.");
    }

    var state = {
      editorId: editorId,
      destroyed: false,
      boundRuntimes: new Set()
    };

    editorStates.add(state);

    function destroyInternal(fromShutdown) {
      if (state.destroyed) {
        if (fromShutdown) {
          return false;
        }
        throw userError("This editor has already been destroyed.");
      }

      Array.from(state.boundRuntimes).forEach(function(runtimeState) {
        runtimeState.editorState = null;
        runtimeState.editorId = 0;
        runtimeState.faceId = 0;
        runtimeState.loadedGauge = null;
        if (runtimeState.contextId) {
          callQuiet("mg_runtime_clear_screen", "number", ["number"], [runtimeState.contextId]);
        }
      });
      state.boundRuntimes.clear();

      var removed = callQuiet("mg_editor_destroy", "number", ["number"], [state.editorId]);
      if (!fromShutdown && !removed) {
        throw userError("Could not release the editor session.");
      }

      editorStates.delete(state);
      state.destroyed = true;
      state.editorId = 0;
      return true;
    }

    var editor = {
      get id() {
        return state.editorId;
      },

      createFace: function(json) {
        ensureEditorAlive(state);

        var result = parseResult(
          "create face",
          call("mg_editor_create_face", "string", ["number", "string"], [state.editorId, toJson(json)])
        );
        return makeHandle(result.id, "face");
      },

      clear: function() {
        ensureEditorAlive(state);
        return call("mg_editor_clear", "number", ["number"], [state.editorId]);
      },

      loadDocument: function(json) {
        ensureEditorAlive(state);
        return parseResult(
          "load document",
          call("mg_editor_load_document", "string", ["number", "string"], [state.editorId, toJson(json)])
        );
      },

      saveDocument: function() {
        ensureEditorAlive(state);
        return parseResult(
          "save document",
          call("mg_editor_save_document", "string", ["number"], [state.editorId])
        );
      },

      getHierarchy: function() {
        ensureEditorAlive(state);
        return parseResult(
          "get hierarchy",
          call("mg_editor_get_hierarchy", "string", ["number"], [state.editorId])
        );
      },

      getHistory: function() {
        ensureEditorAlive(state);
        return parseResult(
          "get history",
          call("mg_editor_get_history", "string", ["number"], [state.editorId])
        );
      },

      getPropertiesMeta: function(id, path) {
        ensureEditorAlive(state);
        return parseResult(
          "get properties meta",
          call("mg_editor_get_properties_meta", "string", ["number", "number", "string"], [
            state.editorId,
            unwrapHandle(id),
            path || ""
          ])
        );
      },

      getProperty: function(id, path) {
        ensureEditorAlive(state);
        return parseResult(
          "get property",
          call("mg_editor_get_property", "string", ["number", "number", "string"], [
            state.editorId,
            unwrapHandle(id),
            path || ""
          ])
        );
      },

      hasNode: function(id) {
        ensureEditorAlive(state);
        return !!call("mg_editor_has_node", "number", ["number", "number"], [state.editorId, unwrapHandle(id)]);
      },

      isFace: function(id) {
        ensureEditorAlive(state);
        return !!call("mg_editor_is_face", "number", ["number", "number"], [state.editorId, unwrapHandle(id)]);
      },

      isElement: function(id) {
        ensureEditorAlive(state);
        return !!call("mg_editor_is_element", "number", ["number", "number"], [state.editorId, unwrapHandle(id)]);
      },

      removeElement: function(id) {
        ensureEditorAlive(state);
        return parseResult(
          "remove element",
          call("mg_editor_remove_element", "string", ["number", "number"], [state.editorId, unwrapHandle(id)])
        );
      },

      copyElement: function(id) {
        ensureEditorAlive(state);
        return parseResult(
          "copy element",
          call("mg_editor_copy_element", "string", ["number", "number"], [state.editorId, unwrapHandle(id)])
        );
      },

      pasteElement: function(parentId, index) {
        ensureEditorAlive(state);
        return parseResult(
          "paste element",
          call("mg_editor_paste_element", "string", ["number", "number", "number"], [
            state.editorId,
            unwrapHandle(parentId),
            Number(index) || 0
          ])
        );
      },

      reorderElement: function(id, index) {
        ensureEditorAlive(state);
        return parseResult(
          "reorder element",
          call("mg_editor_reorder_element", "string", ["number", "number", "number"], [
            state.editorId,
            unwrapHandle(id),
            Number(index) || 0
          ])
        );
      },

      undo: function() {
        ensureEditorAlive(state);
        return !!call("mg_editor_undo", "number", ["number"], [state.editorId]);
      },

      redo: function() {
        ensureEditorAlive(state);
        return !!call("mg_editor_redo", "number", ["number"], [state.editorId]);
      },

      clearClipboard: function() {
        ensureEditorAlive(state);
        return !!call("mg_editor_clear_clipboard", "number", ["number"], [state.editorId]);
      },

      createElement: function(parentId, json) {
        ensureEditorAlive(state);

        var result = parseResult(
          "create element",
          call("mg_editor_create_element", "string", ["number", "number", "string"], [
            state.editorId,
            unwrapHandle(parentId),
            toJson(json)
          ])
        );
        return makeHandle(result.id, "element");
      },

      setProperty: function(id, path, value) {
        ensureEditorAlive(state);

        return parseResult(
          "set property",
          call("mg_editor_set_property", "string", ["number", "number", "string", "string"], [
            state.editorId,
            unwrapHandle(id),
            path,
            toJson(value)
          ])
        );
      },

      destroy: function() {
        return destroyInternal(false);
      }
    };

    Object.defineProperty(editor, "__mgState", {
      value: state,
      enumerable: false
    });

    return editor;
  }

  function shutdown() {
    if (shutdownFlag) {
      return;
    }

    Array.from(editorStates).forEach(function(state) {
      try {
        if (!state.destroyed) {
          state.boundRuntimes.forEach(function(runtimeState) {
            runtimeState.editorState = null;
            runtimeState.editorId = 0;
            runtimeState.faceId = 0;
            runtimeState.loadedGauge = null;
            if (runtimeState.contextId) {
              callQuiet("mg_runtime_clear_screen", "number", ["number"], [runtimeState.contextId]);
            }
          });
          state.boundRuntimes.clear();
          callQuiet("mg_editor_destroy", "number", ["number"], [state.editorId]);
          state.destroyed = true;
        }
      } catch (_) {}
    });

    Array.from(runtimeStates).forEach(function(state) {
      try {
        if (!state.destroyed) {
          state.frameActive = false;
          if (state.frameHandle && typeof root.cancelAnimationFrame === "function") {
            root.cancelAnimationFrame(state.frameHandle);
          }
          state.frameHandle = 0;
          if (state.contextId) {
            callQuiet("mg_runtime_remove_context", "number", ["number"], [state.contextId]);
          }
          state.destroyed = true;
          state.contextId = 0;
          state.canvas = null;
          state.editorState = null;
        }
      } catch (_) {}
    });

    editorStates.clear();
    runtimeStates.clear();
    moduleRef = null;
    shutdownFlag = true;
  }

  var api = {
    init: init,
    shutdown: shutdown,
    createRuntime: createRuntime,
    createEditor: createEditor
  };

  return api;
});
