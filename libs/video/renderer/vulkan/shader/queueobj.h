typedef struct QueueData {
	uint        obj_id;
	uint        next;
} QueueData;

[buffer, set(1), binding(0), coherent]
@block QueueCount {
	uint        numObjects;
	uint        maxObjects;
};

[buffer, set(1), binding(1)]
@block Queue {
	QueueData   queue[];
};

[uniform, set(1), binding(2), coherent]
@image(uint, 2D, Array, Storage, R32ui) queue_heads;
