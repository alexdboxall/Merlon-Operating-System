#pragma once

#include <vfs.h>
#include <common.h>
#include <filesystem.h>

int DemofsMountCreator(struct file* raw_device, struct file** out);