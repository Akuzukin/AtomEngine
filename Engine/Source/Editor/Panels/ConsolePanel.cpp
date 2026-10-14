#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void ConsolePanel::Draw(EditorLayer& editor, Scene& s)
	{
		if (editor.mShowConsole)
		{
			ImGui::Begin(EditorLocalization::Label("Console"));


			ImGui::SetNextItemWidth(160);
			EditorLocalization::Combo("Level", &minimum, "Trace\0Debug\0Info\0Warning\0Error\0Critical\0");
			if (ImGui::Button(EditorLocalization::Label("Clear")))
				ClearLogHistory();
			ImGui::SameLine();
			if (ImGui::Button(EditorLocalization::Label("Flush file")))
				FlushLog();
			filter.Draw("Filter");

			ImGui::Checkbox(EditorLocalization::Label("Auto scroll"), &autoScroll);
			auto file = GetLogFile().string();
			ImGui::TextWrapped("%s", file.c_str());
			ImGui::BeginChild("Messages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			bool bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
			for (auto& entry : GetLogHistory())
			{
				if (int(entry.level) < minimum || !filter.PassFilter(entry.formatted.c_str()))
					continue;
				if (entry.level >= LogLevel::Error)
					ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Color(231, 111, 111));
				else if (entry.level == LogLevel::Warning)
					ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Color(221, 183, 100));
				ImGui::TextUnformatted(entry.formatted.c_str());
				if (entry.level >= LogLevel::Warning)
					ImGui::PopStyleColor();
			}
			if (autoScroll && bottom)
				ImGui::SetScrollHereY(1);
			ImGui::EndChild();
			ImGui::End();
		}
	}
}
