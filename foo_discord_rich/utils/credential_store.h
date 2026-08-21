#pragma once

#include <optional>

namespace drp::credentials
{

std::optional<qwr::u8string> ReadTheAudioDbApiKey();
void WriteTheAudioDbApiKey( qwr::u8string_view apiKey );
bool ClearTheAudioDbApiKey();

} // namespace drp::credentials
