#version 460

layout(location = 0) flat in uint vertex_id;

layout(std430, binding = 1) buffer HeadBuffer {
    uint width;
    uint height;
    uint reserved0;
    uint reserved1;
    uint heads[];
} head_buffer;

struct ABufferNode {
    uint vertex_id;
    uint next;
};

layout(std430, binding = 2) buffer NodePool {
    uint allocation_count;
    uint overflow;
    uint capacity;
    uint reserved;
    ABufferNode nodes[];
} node_pool;

void main() {
    const uint node_index = atomicAdd(node_pool.allocation_count, 1);
    if (node_index >= node_pool.capacity) {
        atomicOr(node_pool.overflow, 1);
        return;
    }

    const uvec2 pixel = uvec2(gl_FragCoord.xy);
    if (pixel.x >= head_buffer.width || pixel.y >= head_buffer.height) {
        return;
    }
    const uint head_index = pixel.y * head_buffer.width + pixel.x;
    node_pool.nodes[node_index].vertex_id = vertex_id;
    node_pool.nodes[node_index].next = atomicExchange(head_buffer.heads[head_index], node_index);
}
