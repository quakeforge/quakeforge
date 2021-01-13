#ifndef __QF_Vulkan_texture_h
#define __QF_Vulkan_texture_h

typedef enum {
	QFV_LUMINANCE,
	QFV_LUMINANCE_ALPHA,
	QFV_RGB,
	QFV_RGBA,
} QFVFormat;

typedef struct scrap_s scrap_t;

scrap_t *QFV_CreateScrap (struct qfv_device_s *device, int size,
						  QFVFormat format);
void QFV_ScrapClear (scrap_t *scrap);
void QFV_DestroyScrap (scrap_t *scrap);
VkImageView QFV_ScrapImageView (scrap_t *scrap) __attribute__((pure));
subpic_t *QFV_ScrapSubpic (scrap_t *scrap, int width, int height);
void QFV_SubpicDelete (subpic_t *subpic);

/** Add an update region to the batch queue.
 *
 * The region to be updated is recorded in the batch queue, space allocated
 * in the staging buffer, and a pointer to the allocated space is returned.
 *
 * \note No data is stransfered. This facilitates writing generated data
 * directly to the staging buffer.
 */
void *QFV_SubpicBatch (subpic_t *subpic, struct qfv_stagebuf_s *stage);

/** Flush all batched subpic updates.
 *
 * The offset in the staging bufffer \a stage is reset to 0. The command
 * buffer is populated with the appropriate image layout barriers and the
 * necessary copy commands.
 *
 * \note The command buffer is neither begun nor ended, nor is it submitted
 * to a queue. This is to maximize flexibility in command buffer usage.
 */
void QFV_ScrapFlush (scrap_t *scrap, struct qfv_stagebuf_s *stage,
					 VkCommandBuffer cmd);

#endif//__QF_Vulkan_texture_h
