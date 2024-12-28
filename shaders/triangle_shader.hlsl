struct render_resources_t
{
    uint position_buffer_index;
    uint color_buffer_index;
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
    StructuredBuffer<float3> color_buffer = ResourceDescriptorHeap[render_resources.color_buffer_index];

    vs_out_t result;
    result.position = float4(position_buffer[vertex_id], 1.0f);
    result.color = float4(color_buffer[vertex_id], 1.0f);

    return result;
}

float4 ps_main(vs_out_t ps_input) : SV_Target
{
    return ps_input.color;
}