#pragma once

#include <vfs.h>
#include <common.h>
#include <filesystem.h>

int FatFsMountCreator(struct open_file* raw_device, struct open_file** out);