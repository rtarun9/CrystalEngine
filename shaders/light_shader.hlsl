struct render_resources_t
{
    uint position_buffer_index;
    uint transform_buffer_index;
    uint scene_buffer_index;
};

struct scene_buffer_t
{
    float4 light_position;
    float4 light_color;
};

struct transform_buffer_t
{
    float4x4 model_matrix;
    float4x4 view_projection_matrix;
};

ConstantBuffer<render_resources_t> render_resources : register(b0);

struct vs_out_t
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

vs_out_t vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> position_buffer = ResourceDescriptorHeap[render_resources.position_buffer_index];

    ConstantBuffer<transform_buffer_t> transform_buffer =
        ResourceDescriptorHeap[render_resources.transform_buffer_index];

    ConstantBuffer<scene_buffer_t> scene_buffer = ResourceDescriptorHeap[render_resources.scene_buffer_index];

    vs_out_t result;

    float4x4 mvp_matrix = mul(transform_buffer.model_matrix, transform_buffer.view_projection_matrix);
    result.position = mul(float4(position_buffer[vertex_id], 1.0f), mvp_matrix);

    result.color = float4(scene_buffer.light_color);

    return result;
}

float4 ps_main(vs_out_t ps_input) : SV_Target
{
    return ps_input.color;
}
