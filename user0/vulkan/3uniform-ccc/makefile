mac:shader
	clang test.c -lvulkan -lglfw -lm
win:shader
	gcc test.c -lvulkan-1 -lglfw3 -lgdi32 -lm
linux:shader
	gcc test.c -I/opt/vulkan/x86_64/include -L/opt/vulkan/x86_64/lib -lvulkan -lglfw3 -lX11 -ldl -lm -lpthread
shader:
	glslc shader.vert -o shader.vert.spv
	glslc shader.frag -o shader.frag.spv
clean:
	rm *.out *.spv
