#include "ResourcePool.h"
#include "../../utils/ResourceManager/ResourceManager.h"

static ResourceManager Font_;
static ResourceManager Image_;

void ResourcePool::Init()
{
    Font_.SetDefault((void *)LV_FONT_DEFAULT);
}

lv_font_t *ResourcePool::GetFont(const char *name)
{
    return (lv_font_t *)Font_.GetResource(name);
}

const void *ResourcePool::GetImage(const char *name)
{
    return Image_.GetResource(name);
}
