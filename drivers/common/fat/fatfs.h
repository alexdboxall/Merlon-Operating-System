#pragma once

#include <vfs.h>
#include <common.h>
#include <filesystem.h>

int FatFsMountCreator(struct file* raw_device, struct file** out);