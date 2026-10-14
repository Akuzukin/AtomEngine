#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void HelpPanel::Draw(EditorLayer& editor, Scene& s)
	{
		if (editor.mShowHelp)
		{
			ImGui::Begin(EditorLocalization::Label("Editor help"), &editor.mShowHelp);
			ImGui::TextUnformatted(EditorLocalization::Text(
				"Ctrl+S Save | Ctrl+Shift+S Save As | Ctrl+O Open | Ctrl+N New\nCtrl+Z Undo | Ctrl+Y / Ctrl+Shift+Z Redo | "
				"Ctrl+D Duplicate\nDelete Delete tree | F Frame selected | Escape Deselect\nT Move | R Rotate | "
				"S Scale (viewport)\nAlt+LMB Orbit | MMB Pan | RMB Zoom | Shift Fly\nDrag objects to change parent; drop "
				"on Scene root to detach.\nDrag assets to viewport or compatible Inspector fields.\nPlay uses an isolated "
				"scene snapshot; Stop restores edit state."));
			ImGui::End();
		}
	}
}
