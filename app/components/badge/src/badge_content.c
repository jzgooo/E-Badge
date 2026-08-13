#include "badge.h"

static const char *s_title = "E-Badge";
static const char *s_subtitle = "Hello";

const char *badge_content_get_title(void)
{
    return s_title;
}

const char *badge_content_get_subtitle(void)
{
    return s_subtitle;
}
