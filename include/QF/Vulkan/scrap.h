#ifndef __QF_Vulkan_scrap_h
#define __QF_Vulkan_scrap_h

#include "QF/image.h"

typedef struct scrap_s scrap_t;

struct qfv_stagebuf_s;
struct qfv_device_s;

scrap_t *QFV_CreateScrap (struct qfv_device_s *device, int size,
						  QFFormat format, struct qfv_stagebuf_s *stage);
size_t QFV_ScrapSize (scrap_t *scrap);
void QFV_ScrapClear (scrap_t *scrap);
void QFV_DestroyScrap (scrap_t *scrap);
VkImageView QFV_ScrapImageView (scrap_t *scrap) __attribute__((pure));
subpic_t *QFV_ScrapSubpic (scrap_t *scrap, int width, int height);
void QFV_SubpicDelete (subpic_t *subpic);

void *QFV_SubpicBatch (subpic_t *subpic, struct qfv_stagebuf_s *stage);

void QFV_ScrapFlush (scrap_t *scrap);

#endif//__QF_Vulkan_scrap_h
