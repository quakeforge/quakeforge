#ifndef __QF_Vulkan_scrap_h
#define __QF_Vulkan_scrap_h

#include "QF/image.h"

typedef struct scrap_s scrap_t;
typedef struct subpic_s subpic_t;
typedef struct scrapbox_s scrapbox_t;
typedef struct rscrap_s rscrap_t;
typedef struct qfv_stagebuf_s qfv_stagebuf_t;
typedef struct qfv_device_s qfv_device_t;

scrap_t *QFV_CreateScrap (qfv_device_t *device, const char *name,
						  int size, QFFormat format, qfv_stagebuf_t *stage);
scrap_t *QFV_CreateScrapFromScrap (qfv_device_t *device, const char *name,
								   //copies contents of rscrap, don't use later
								   rscrap_t *rscrap, QFFormat format,
								   qfv_stagebuf_t *stage);
uint32_t QFV_ScrapLayers (scrap_t *scrap) __attribute__((pure));
float QFV_ScrapSize (scrap_t *scrap) __attribute__((pure));
void QFV_ScrapClear (scrap_t *scrap);
void QFV_DestroyScrap (scrap_t *scrap);
VkImageView QFV_ScrapImageView (scrap_t *scrap) __attribute__((pure));
VkImage QFV_ScrapImage (scrap_t *scrap) __attribute__((pure));
subpic_t *QFV_ScrapSubpic (scrap_t *scrap, int width, int height);
void QFV_SubpicDelete (subpic_t *subpic);

void *QFV_SubpicBatch (subpic_t *subpic, qfv_stagebuf_t *stage);

void QFV_ScrapFlush (scrap_t *scrap);

#endif//__QF_Vulkan_scrap_h
