## Biggest gaps (library perspective)

1. ~~No high-level draw API~~
 - ~~Users still do: find pipeline → bindPipeline → pick descriptor_sets[i] → sync uniforms → draw.~~
 - ~~A toolkit wants: draw(pipeline, mesh, instance_count) / draw_mesh_tasks(pipeline, groups).~~

2. ~~Uniform/SSBO sync is still “raw Vulkan memory”~~
 - ~~sync_uniform(DeviceMemory, ...) and digging into pipeline.ubos[type].memos[idx] force users into buffer internals.~~
 - ~~Want: sync_ubo(pipeline, UBOType_Camera, data, frame) / typed SSBO sync by name or enum.~~

3. Pass control is rigid
 - ~~record_cmds hardcodes single color attach, clear color/depth, load=Clear. No load/store policy, no MRT.~~ No “render offscreen then blit to swapchain”.
 - That’s the main blocker for deferred, post, ping-pong, and real multi-pass graphs.

4. ~~Closed UBO/SSBO type registry~~
 - ~~Reflection only accepts names mapped to enums in common.h. New materials mean editing Context internals.~~
 - ~~A toolkit should accept reflected names (or a registration table) without library source changes.~~

5. ~~Resize / lifetime holes~~
 - ~~recreate_swapchain() clears depth_attachments; no public extent API; custom RTs/depth not safely regenerated. Shadow maps break on resize unless the app knows too much.~~

6. Incomplete “common viewport features”

 - MSAA + resolve (flag exists, path incomplete)
 - Mipmaps / texture bind API (samplers without tex_img_pairs stay unbound)
 - Compute storage images rejected
 - ~~No push constants, indirect draw~~, GPU timers, screenshot/readback

7. ~~Frame API shape is awkward~~
 - ~~Recording often lives inside update_cbk mid-draw_frame. Cleaner model: update → record passes → submit/present as separate steps.~~

8. Too much Vulkan leaks through public surface
 - Public pipelines / meshes / PipelineOption as raw create-infos means the “easy path” still looks like Vulkan. Fine for escape hatches; bad as the default API.

## Priority if the goal is “focus on rendering features”

Priority	Add next
P0
~~Named bind/draw + typed UBO/SSBO sync~~
P0
Pass params (clear/load/store, skip-present / blit)
P1
~~Resize lifecycle for RTs/depth + public size API~~
P1
~~Open buffer-type registration (not closed enums only)~~
P1
~~Pipeline color formats + MRT~~
P2
Texture bind + mipmaps; MSAA resolve; multi-pass helpers
P2
Compute storage images + better barriers
P3
~~Push constants, indirect~~, queries, readback

## Examples

Here are feature → effect pairs: each library gap is driven by a real rendering feature you’d implement at the same time, so the API stays grounded.

1. ~~High-level draw API~~

~~Effect: Instanced Cornell / opaque batching (you already have this, just cleaner)~~

// After API:
ctx.sync_ubo("phong", UBOType_Camera, &cam, frame);
ctx.draw("phong", "cornell_cube", instance_count);
// Instead of bindPipeline + descriptor_sets[i] + draw_mesh_instanced
Also good driver: wireframe mesh-shader path → ctx.draw_mesh_tasks("wireframe", gx, gy, gz).

2. ~~Typed UBO / SSBO sync~~

~~Effect: Forward+ light clustering (camera + cluster params + light SSBO every frame)~~

ctx.sync_ubo("phong_plus", UBOType_Camera, &camera_ubo, frame);
ctx.sync_ubo("phong_plus", UBOType_ClusterParams, &cluster_params, frame);
ctx.sync_ssbo("clusterize", SSBOType_PointLights, lights.data(), frame);
ctx.record_compute(cmd, "clusterize", groups_x, groups_y, groups_z);
Same API later serves material batches, shadow matrices, post params.

3. Pass control (clear / load-store / blit)

Effect: Tone mapping / FXAA post pass

// Pass A: scene → offscreen HDR RT (clear)
ctx.begin_pass({.target = hdr_rt, .clear_color = {0,0,0,1}, .load = Clear});
ctx.draw("opaque", ...);
ctx.end_pass();
// Pass B: HDR → swapchain (load don't care, or blit)
ctx.begin_pass({.target = Swapchain, .load = DontCare});
ctx.draw("tonemap", "fullscreen_tri", 1);
// or: ctx.blit(hdr_rt, Swapchain);
Also drives: ping-pong blur (bloom) — Pass N loads previous RT, writes next.

4. ~~Open UBO/SSBO registration~~

~~Effect: New PBR material without editing Context~~

// User registers once (or reflection just accepts the GLSL block name)
ctx.register_ubo_type("PBRMaterialUBO", sizeof(PBRMaterialUBO));
ctx.register_ssbo_type("JointMatrices", sizeof(glm::mat4));
// Shader uses layout(binding=N) uniform PBRMaterialUBO { ... };
// Context auto-allocates; no new enum required in common.h
Driver effect: skinned mesh (joint matrix SSBO) or custom toon material UBO.

5. ~~Resize / lifetime for RTs & depth~~

~~Effect: Windowed shadow + SSAO that survive resize~~

ctx.set_resize_cbk([&](uint32_t w, uint32_t h) {
    ctx.resize_depth_attachment(shadow_idx, shadow_map_size, shadow_map_size); // or keep fixed
    ctx.resize_render_target(ssao_rt, w, h);
    ctx.resize_render_target(hdr_rt, w, h);
});
auto [w, h] = ctx.extent(); // public size API
Without this, every “nice” effect that uses custom targets breaks on resize.

6a. ~~Pipeline color formats + MRT~~

~~Effect: Deferred G-buffer~~

ctx.add_render_target(..., vk::Format::eR16G16B16A16Sfloat); // albedo
ctx.add_render_target(..., vk::Format::eR16G16B16A16Sfloat); // normal
ctx.add_render_target(..., vk::Format::eR16G16Sfloat);       // motion / roughness-metal
ctx.create_pipeline("gbuffer", pack, opt, comps,
    {.color_formats = {albedo_fmt, normal_fmt, mat_fmt}});
ctx.begin_pass({.targets = {albedo, normal, mat}, .depth = scene_depth});
ctx.draw("gbuffer", mesh, n);
ctx.end_pass();
ctx.begin_pass({.target = Swapchain});
ctx.draw("deferred_lighting", "fullscreen_tri", 1); // samples G-buffer
6b. Texture bind + mipmaps
Effect: Textured PBR / environment lighting

ctx.add_texture("albedo", "brick.png", {.gen_mips = true});
ctx.bind_texture("pbr", /*binding*/3, "albedo");
ctx.bind_cubemap("pbr", /*binding*/4, "sky_irradiance");
Driver: specular IBL needs mipmapped cubemap; today you can’t express that cleanly.

6c. MSAA + resolve
Effect: Smooth viewport edges (forward opaque)

ctx.set_msaa(vk::SampleCountFlagBits::e4);
// Context creates MSAA color/depth, resolves into swapchain or post RT
ctx.begin_pass({.msaa = true, .resolve_to = Swapchain});
ctx.draw("opaque", ...);
6d. Multi-pass helpers
Effect: Cascaded / single-map shadows (you’re halfway there)

ctx.shadow_pass(shadow_depth_idx, [&](auto& cmd) {
    ctx.draw(cmd, "shadow_depth", "cornell_cube", n);
});
ctx.main_pass([&](auto& cmd) {
    ctx.bind_depth_attachment("phong_shadow", binding, shadow_depth_idx);
    ctx.draw(cmd, "phong_shadow", "cornell_cube", n);
});
Wraps record_depth_pass + layout transitions + descriptor bind.

7. Compute storage images + barriers

Effect: Compute blur / SSAO / light culling writing an image

ctx.add_storage_image("ssao_raw", w, h, R16_SFLOAT);
ctx.bind_storage_image("ssao_comp", binding, "ssao_raw");
ctx.record_compute(cmd, "ssao_comp", gx, gy, 1);
ctx.barrier_image("ssao_raw", ComputeWrite → FragmentSample);
ctx.draw("ssao_blur_or_composite", "fullscreen_tri", 1);

8. ~~Frame API: update → record → submit~~

~~Effect: Any multi-pass frame (shadow + cluster + opaque + post + HUD)~~

while (!should_close) {
    ctx.begin_frame();                    // acquire
    float dt = ctx.delta_time();
    update_camera(dt);
    ctx.sync_ubo(...);                    // CPU → GPU for this frame index
    ctx.record([&](Frame& f) {
        f.shadow_pass(...);
        f.compute("clusterize", ...);
        f.main_pass(...);
        f.post("tonemap", ...);
        hud.render(f.cmd);
    });
    ctx.end_frame();                      // submit + present
}
Today’s update_cbk inside draw_frame fights this structure.

9. ~~Push constants / indirect~~ / queries / readback (P3)

API	Effect example
~~Push constants~~
~~Per-draw tint / object ID without a UBO update~~
~~Indirect draw~~
~~GPU culling: compute fills DrawIndirectCommand, graphics draws visible only~~
Timestamp queries
HUD: “shadow 0.4ms / opaque 2.1ms / post 0.3ms”
Readback / screenshot
Save viewport PNG, or CPU pick buffer for editor selection
ctx.push_constants("debug", { .object_id = id });
ctx.draw_indirect("opaque", "indirect_cmds");
auto timings = ctx.query_pass_times();
ctx.screenshot("frame.png"); // or read picking RT
Suggested implement order (feature + effect together)
~~Draw + sync API → refactor Cornell / Forward+ demo (no new look, cleaner API)~~
Pass params + blit → HDR + tonemap (visible win)
~~Resize lifecycle → keep tonemap/shadow alive on window resize~~
~~MRT → deferred or SSAO G-buffer~~
Mipmaps + bind_texture → textured PBR
MSAA resolve → quality polish
Storage images → compute SSAO/blur
Frame API cleanup → once multi-pass is painful enough
Indirect / queries → GPU culling + perf HUD