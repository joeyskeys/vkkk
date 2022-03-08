## Vk Notes

### Concepts

- Instance, PhysicalDevice, LogicalDevice

  Vk still works like a state machine, and Instance is the handler of the machine;

  You can iterate through PhysicalDevices with Instance, which correspond to your physical GPU, and Vk has the ability to exploit multiple GPUs;

  LogicalDevice is the programming handle to create all other Vk objects, which is created from PhysicalDevice. Multiple LogicalDevices could be created from one PhysicalDevice.

- Surface, framebuffer

  Vk doesn't handle the window system directly, just like OpenGL. So you need to get the window surface through APIs like `glfwCreateWindowSurface`;

  framebuffer is the actual GPU buffer to store the image to be displayed, Vk doesn't provide default framebuffer, you have to create it explicitly;

- Swapchain, VkImage, VkImageViews

  Vk present images like a pipeline, swapchain is literally a image queue the GPU renders images into, it contains VkImages;

  VkImageViews are non-owning view of the VkImages in swapchain, just like the span concept in cpp;

- RenderPipeline, RenderPass, ShaderModule, ShaderStage, FixedFunctionState

  RenderPipeline in Vk must be created explicitly, it consists of RenderPass, ShaderStages InputInformation and FixedFunctionState;

  ShaderStage is like linked program in OpenGL, the actual used shader, created by supplying ShaderModules;

  ShaderModule is just like Shader in OpenGL but using the SPIR-V code (Interestingly, raw shader could also work..)

  FixedFunctionState are just like the concept in OpenGL, including multisampling, blending and etc. It also provide something not presented in OpenGL like rasterizer and scisor setting;

- CommandBuffer, DrawingCmd and SyncObjects

  You need to record the drawing commands first and then dispatch it for the real draw which allow the driver to do better optimization, CommandBuffer is the object to store recorded commands;

- DrawingCmd is just like the DrawCall in other APIs;

- SyncObjects includes semaphore and fence coz Vk rendering are all parallel and sometimes you need to do some synchronization. Semaphores are used to sync between gpu operations, fences are used to sync with host a.k.a CPU side.