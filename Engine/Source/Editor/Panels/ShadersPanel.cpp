#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void ShadersPanel::Draw(EditorLayer& editor, Scene& s)
	{
		if (editor.mShowShaders)
		{
			ImGui::Begin(EditorLocalization::Label("Shaders"), &editor.mShowShaders);
			if (!ShaderLibrary::Error().empty())
				ImGui::TextWrapped("%s", ShaderLibrary::Error().c_str());
			for (auto& shader : ShaderLibrary::All())
			{
				auto name = WStringToUTF8(shader->File());
				ImGui::PushID(shader.get());
				if (ImGui::TreeNode(name.c_str()))
				{
					ImGui::Text(EditorLocalization::Text("Revision: %llu"), shader->Revision());
					for (auto& binding : shader->Bindings())
						ImGui::Text(EditorLocalization::Text("%s : register %u, space %u"), binding.name.c_str(),
							binding.bindPoint, binding.space);
					ImGui::BeginDisabled(!shader->preview);
					if (ImGui::Button(EditorLocalization::Label("Preview")))
						shader->RequestPreview({});
					ImGui::EndDisabled();
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::End();
		}
	}
}
