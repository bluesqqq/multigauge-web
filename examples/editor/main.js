(function(root) {
  function createFaceJson() {
    return {
      layout: {
        "flex-container": {
          direction: "column",
          justify: "center",
          "align-items": "center"
        },
        padding: {
          left: 24,
          right: 24,
          top: 24,
          bottom: 24
        }
      },
      children: []
    };
  }

  function createRectangleJson() {
    return {
      type: "rectangle",
      style: {
        width: "60%",
        height: 220,
        "flex-container": {
          direction: "column",
          justify: "center",
          "align-items": "center"
        }
      },
      props: {
        paint: {
          fill: {
            color: "#22c55e"
          }
        },
        radius: 14
      },
      children: []
    };
  }

  async function bootstrap() {
    try {
      if (!root.Multigauge || typeof root.Multigauge.init !== "function") {
        throw new Error("Multigauge is not available.");
      }

      var mg = await root.Multigauge.init();
      var canvas = document.getElementById("screen");
      if (!canvas) {
        throw new Error("Canvas #screen was not found.");
      }

      var runtime = mg.createRuntime(canvas);
      var editor = mg.createEditor();
      var face = editor.createFace(createFaceJson());
      var element = editor.createElement(face, createRectangleJson());

      runtime.bindEditor(editor, face);

      if (!root.MGEditorApp || typeof root.MGEditorApp.initEditorUI !== "function") {
        throw new Error("Editor UI modules were not loaded.");
      }

      root.MGEditorApp.initEditorUI({
        runtime: runtime,
        editor: editor,
        canvas: canvas,
        face: face,
        selectedId: element.id
      });

      runtime.start();
    } catch (err) {
      console.error("Multigauge editor harness failed to start:", err);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bootstrap);
  } else {
    bootstrap();
  }
})(typeof globalThis !== "undefined" ? globalThis : window);
