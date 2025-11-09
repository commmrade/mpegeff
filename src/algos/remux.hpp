#pragma once
#include "formatcontext.hpp"
#include <filesystem>

void remux(std::filesystem::path input, std::filesystem::path output, std::string_view out_ctr);
