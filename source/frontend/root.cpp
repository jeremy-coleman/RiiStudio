#include "root.hpp"
#include <core/3d/gl.hpp>
#include <core/api.hpp>
#include <core/util/gui.hpp>
#include <core/util/timestamp.hpp>
#include <frontend/Localization.hpp>
#include <frontend/editor/EditorWindow.hpp>
#include <frontend/widgets/fps.hpp>
#include <frontend/widgets/fullscreen.hpp>
#include <frontend/widgets/theme_editor.hpp>
#include <fstream>
#include <imgui_markdown.h>
#include <oishii/reader/binary_reader.hxx>
#include <oishii/writer/binary_writer.hxx>
#include <pfd/portable-file-dialogs.h>

// Experimental conversion
#include <plugins/g3d/collection.hpp>
#include <plugins/j3d/Scene.hpp>

#include <vendor/stb_image.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef BUILD_ASAN
#include <sanitizer/lsan_interface.h>
#endif

namespace llvm {
int DisableABIBreakingChecks;
} // namespace llvm

bool gIsAdvancedMode = false;

bool IsAdvancedMode() { return gIsAdvancedMode; }

namespace riistudio::frontend {

RootWindow* RootWindow::spInstance;

#ifdef _WIN32
static void GlCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                       GLsizei length, const GLchar* message,
                       GLvoid* userParam) {
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    return;

  printf("%s\n", message);
}
#endif

void RootWindow::draw() {
  fileHostProcess();

  ImGui::PushID(0);
  if (BeginFullscreenWindow("##RootWindow", getOpen())) {
    if (mThemeUpdated) {
      mTheme.setThemeEx(mCurTheme);
      mThemeUpdated = false;
    }
    ImGui::GetIO().FontGlobalScale = mFontGlobalScale;

    ImGui::SetWindowFontScale(1.1f);
    if (!hasChildren()) {
      ImGui::Text("Drop a file to edit.");
    }
    // ImGui::Text(
    //     "Note: For this early alpha, file saving is temporarily disabled.");
    ImGui::SetWindowFontScale(1.0f);
    dockspace_id = ImGui::GetID("DockSpaceWidget");

    // ImGuiID dock_main_id = dockspace_id;
    while (mAttachEditorsQueue.size()) {
      const std::string& ed_id = mAttachEditorsQueue.front();
      (void)ed_id;

      // ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing
      // layout ImGui::DockBuilderAddNode(dockspace_id); // Add empty node
      //
      //
      // ImGuiID dock_up_id = ImGui::DockBuilderSplitNode(dock_main_id,
      // ImGuiDir_Up, 0.05f, nullptr, &dock_main_id);
      // ImGui::DockBuilderDockWindow(ed_id.c_str(), dock_up_id);
      //
      //
      // ImGui::DockBuilderFinish(dockspace_id);
      mAttachEditorsQueue.pop();
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), 0);

    EditorWindow* ed =
        getActive() ? dynamic_cast<EditorWindow*>(getActive()) : nullptr;
    assert(ed || !hasChildren());

    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File"_j)) {
#if !defined(__EMSCRIPTEN__)
        if (ImGui::MenuItem("Open"_j)) {
          openFile();
        }
#endif
        if (ImGui::MenuItem("Save"_j)) {

          if (ed) {
            DebugReport("Attempting to save to %s\n",
                        ed->getFilePath().c_str());
            if (!ed->getFilePath().empty())
              save(ed->getFilePath());
            else
              saveAs();
          } else {
            printf("Cannot save.. nothing has been opened.\n"_j);
          }
        }
#if !defined(__EMSCRIPTEN__)
        if (ImGui::MenuItem("Save As"_j)) {
          if (ed)
            saveAs();
          else
            printf("Cannot save.. nothing has been opened.\n"_j);
        }
#endif
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Settings"_j)) {
        bool _vsync = vsync;
        ImGui::Checkbox("VSync"_j, &_vsync);

        if (_vsync != vsync) {
          setVsync(_vsync);
          vsync = _vsync;
        }

        mThemeUpdated |= DrawThemeEditor(mCurTheme, mFontGlobalScale, nullptr);

#ifdef BUILD_DEBUG
        ImGui::Checkbox("ImGui Demo", &bDemo);
#endif

        ImGui::Checkbox("Advanced Mode"_j, &gIsAdvancedMode);

        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("日本語")) {
        static int locale = 0;

        int locale_opt = locale;
        ImGui::RadioButton("English", &locale_opt, 0);
        ImGui::RadioButton("日本語", &locale_opt, 1);

        if (locale_opt != locale) {
          locale = locale_opt;

          const char* id_str = "English";
          switch (locale) {
          default:
          case 0:
            id_str = "English";
            break;
          case 1:
            id_str = "Japanese";
            break;
          }

          riistudio::SetLocale(id_str);
        }

        if (ImGui::Button("Dump Untranslated Strings"_j)) {
          riistudio::DebugDumpLocaleMisses();
        }
        if (ImGui::Button("Reload Language .csv Files"_j)) {
          riistudio::DebugReloadLocales();
		}

        ImGui::EndMenu();
      }

      if (bDemo)
        ImGui::ShowDemoWindow(&bDemo);

      ImGui::SameLine(ImGui::GetWindowWidth() - 60);
      DrawFps();

      ImGui::EndMenuBar();
    }

    if (mCheckUpdate)
      mUpdater.draw();

    if (!mImportersQueue.empty()) {
      auto& window = mImportersQueue.front();
      if (window.attachEditor()) {
        attachEditorWindow(std::make_unique<EditorWindow>(window.takeResult(),
                                                          window.getPath()));
        mImportersQueue.pop();
      } else if (window.abort()) {
        mImportersQueue.pop();
      } else {
        const auto wflags = ImGuiWindowFlags_NoCollapse;

        ImGui::OpenPopup("Importer"_j);

        ImGui::SetNextWindowSize({800.0f, 00.0f});
        ImGui::BeginPopupModal("Importer"_j, nullptr, wflags);
        window.draw();
        ImGui::EndPopup();
      }
    }
    drawChildren();
  }
  // Handle popups
  EndFullscreenWindow();
  ImGui::PopID();

#ifdef BUILD_ASAN
  __lsan_do_leak_check();
#endif
}
void RootWindow::onFileOpen(FileData data, OpenFilePolicy policy) {
  printf("Opening file: %s\n", data.mPath.c_str());

  if (mWantFile) {
    mReqData = std::move(data);
    mGotFile = true;
    return;
  }

  if (!mImportersQueue.empty()) {
    auto& top = mImportersQueue.front();

    if (top.acceptDrop()) {
      top.drop(std::move(data));
      return;
    }
  }

  mImportersQueue.emplace(std::move(data));
}
void RootWindow::attachEditorWindow(std::unique_ptr<EditorWindow> editor) {
  mAttachEditorsQueue.push(editor->getName());
  attachWindow(std::move(editor));
}

RootWindow::RootWindow()
    : Applet(std::string("RiiStudio "_j) + RII_TIME_STAMP) {
#ifdef _WIN32
  glDebugMessageCallback(GlCallback, 0);
#endif
  spInstance = this;

  InitAPI();

  ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
#ifdef RII_BACKEND_GLFW
  GLFWwindow* window = reinterpret_cast<GLFWwindow*>(getPlatformWindow());
  GLFWimage image;
  image.pixels = stbi_load("icon.png", &image.width, &image.height, 0, 4);
  glfwSetWindowIcon(window, 1, &image);
  stbi_image_free(image.pixels);
#endif
}
RootWindow::~RootWindow() { DeinitAPI(); }

void RootWindow::save(const std::string& path) {
  EditorWindow* ed =
      getActive() ? dynamic_cast<EditorWindow*>(getActive()) : nullptr;
  if (ed != nullptr)
    ed->saveAs(path);
}
void RootWindow::saveAs() {
  std::vector<std::string> filters;

  EditorWindow* ed =
      getActive() ? dynamic_cast<EditorWindow*>(getActive()) : nullptr;
  if (ed == nullptr)
    return;

  const kpi::INode* node = &ed->getDocument().getRoot();

  if (dynamic_cast<const riistudio::j3d::Collection*>(node) != nullptr) {
    filters.push_back("Binary Model Data (*.bmd)"_j);
    filters.push_back("*.bmd");
  } else if (dynamic_cast<const riistudio::g3d::Collection*>(node) != nullptr) {
    filters.push_back("Binary Resource (*.brres)"_j);
    filters.push_back("*.brres");
  }

  filters.push_back("All Files");
  filters.push_back("*");

  auto results = pfd::save_file("Save File"_j, "", filters).result();
  if (results.empty())
    return;

  if (dynamic_cast<const riistudio::j3d::Collection*>(node) != nullptr) {
    if (!results.ends_with(".bmd"))
      results.append(".bmd");
  } else if (dynamic_cast<const riistudio::g3d::Collection*>(node) != nullptr) {
    if (!results.ends_with(".brres"))
      results.append(".brres");
  }

  save(results);
}

} // namespace riistudio::frontend
