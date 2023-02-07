#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <time.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "EW/Shader.h"
#include "EW/ShapeGen.h"

void resizeFrameBufferCallback(GLFWwindow* window, int width, int height);
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);

float lastFrameTime;
float deltaTime;

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 720;

double prevMouseX;
double prevMouseY;
bool firstMouseInput = false;

/* Button to lock / unlock mouse
* 1 = right, 2 = middle
* Mouse will start locked. Unlock it to use UI
* */
const int MOUSE_TOGGLE_BUTTON = 1;
const float MOUSE_SENSITIVITY = 0.1f;

glm::vec3 bgColor = glm::vec3(0);
float orbitRadius = 0.0f;
float orbitSpeed = 0.0f;
float fieldOfView = 60.0f;
float orthographicHeight = 1.0f;
bool isOrthographic = false;

struct Transform
{
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1);

	glm::mat4 translationMatrix(glm::vec3 p)
	{
		return glm::mat4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			p.x, p.y, p.z, 1
		);
	}

	glm::mat4 rotationMatrix(glm::vec3 r)
	{
		glm::mat4 eulerX = glm::mat4
		(
			1, 0, 0, 0,
			0, cos(r.x), sin(r.x), 0,
			0, -sin(r.x), cos(r.x), 0,
			0, 0, 0, 1
		);

		glm::mat4 eulerY = glm::mat4
		(
			cos(r.y), 0, -sin(r.y), 0,
			0, 1, 0, 0,
			sin(r.y), 0, cos(r.y), 0,
			0, 0, 0, 1
		);

		glm::mat4 eulerZ = glm::mat4
		(
			cos(r.z), sin(r.z), 0, 0,
			-sin(r.z), cos(r.z), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		return eulerX * eulerY * eulerZ;
	}

	glm::mat4 scaleMatrix(glm::vec3 s)
	{
		return glm::mat4
		(
			s.x, 0, 0, 0,
			0, s.y, 0, 0,
			0, 0, s.z, 0,
			0, 0, 0, 1
		);
	}

	glm::mat4 getModelMatrix() { return translationMatrix(position) *
										rotationMatrix(rotation) *
										scaleMatrix(scale); };
};

struct Camera
{
	glm::vec3 position;
	glm::vec3 target;
	float fov;
	float orthographicSize;
	bool isOrthographic;

	glm::mat4 orthographic(float height, float aspectRatio, float nearPlane, float farPlane)
	{
		// Bounds
		float right = height * aspectRatio;
		float left = -right;
		float top = height;
		float bottom = -top;

		glm::mat4 orthoMat = glm::mat4
		(
			2/(right - left), 0, 0, 0,
			0, 2/(top - bottom), 0, 0,
			0, 0, -2/(farPlane - nearPlane), 0,
			-(right + left)/(right - left), -(top + bottom)/(top - bottom), -(farPlane + nearPlane)/(farPlane - nearPlane), 1
		);

		return orthoMat;
	}

	glm::mat4 perspective(float fov, float aspectRatio, float nearPlane, float farPlane)
	{
		float c = tan(glm::radians(fov) / 2);
		float near = nearPlane;
		float far = farPlane;

		glm::mat4 perspMat = glm::mat4
		(
			1/(aspectRatio * c), 0, 0, 0,
			0, 1/c, 0, 0,
			0, 0, -((far + near)/(far - near)), -1,
			0, 0, -((2 * far * near)/(far - near)), 1
		);

		return perspMat;
	}

	glm::mat4 getViewMatrix()
	{
		// Orthonomal Vectors via Gram-Schmidt Process
		glm::vec3 forward = glm::normalize(target - position);
		glm::vec3 up = glm::vec3(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(up, forward));
		up = glm::normalize(glm::cross(right, forward));
		forward = -forward;

		// View Rotation
		glm::mat4 rotMatrix = glm::mat4
		(
			right.x, up.x, forward.x, 0,
			right.y, up.y, forward.y, 0,
			right.z, up.z, forward.z, 0,
			0, 0, 0, 1
		);

		// Inverse of translation to return it to world origin
		glm::mat4 inverseTranslation = glm::mat4
		(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			-position.x, -position.y, -position.z, 1
		);

		// Concantenation of rotation and inverse translation
		// gets the view matrix
		return rotMatrix * inverseTranslation;
	}

	glm::mat4 getProjectionMatrix()
	{
		if (isOrthographic)
		{
			return orthographic(orthographicHeight, SCREEN_WIDTH / SCREEN_HEIGHT, 0, 100);
		}

		return perspective(fov, SCREEN_WIDTH / SCREEN_HEIGHT, 0, 100);
	}
};

int main() {
	srand(time(0));

	if (!glfwInit()) {
		printf("glfw failed to init");
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Transformations", 0, 0);
	glfwMakeContextCurrent(window);

	if (glewInit() != GLEW_OK) {
		printf("glew failed to init");
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, resizeFrameBufferCallback);
	glfwSetKeyCallback(window, keyboardCallback);

	// Setup UI Platform/Renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	//Dark UI theme.
	ImGui::StyleColorsDark();

	Shader shader("shaders/vertexShader.vert", "shaders/fragmentShader.frag");

	MeshData cubeMeshData;
	createCube(1.0f, 1.0f, 1.0f, cubeMeshData);

	Mesh cubeMesh(&cubeMeshData);

	Transform randomTransforms[10];

	for (int i = 0; i < 10; i++)
	{
		Transform randomTransform;

		float randPos[3];

		for (int i = 0; i < 3; i++)
		{
			randPos[i] = (-5) + static_cast<float>(rand()) / static_cast<float>(RAND_MAX / (5 - -5));
		}

		randomTransform.position = glm::vec3(randPos[0], randPos[1], randPos[2]);
		randomTransform.rotation = glm::vec3(rand() % 360);
		randomTransform.scale = glm::vec3((rand() % 3) + 1);

		randomTransforms[i] = randomTransform;
	}

	Camera camera;

	//Enable back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	while (!glfwWindowShouldClose(window)) {
		glClearColor(bgColor.r,bgColor.g,bgColor.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float time = (float)glfwGetTime();
		deltaTime = time - lastFrameTime;
		lastFrameTime = time;

		//Draw
		shader.use();

		camera.position = glm::vec3(cos(time * orbitSpeed) * orbitRadius, 0, sin(time * orbitSpeed) * orbitRadius);
		camera.target = glm::vec3(0, 0, 0);
		camera.fov = fieldOfView;
		camera.orthographicSize = orthographicHeight;
		camera.isOrthographic = isOrthographic;

		for (int i = 0; i < 10; i++)
		{
			shader.setMat4("transform", randomTransforms[i].getModelMatrix());
			shader.setMat4("view", camera.getViewMatrix());
			shader.setMat4("projection", camera.getProjectionMatrix());
			cubeMesh.draw();
		}

		//Draw UI
		ImGui::Begin("Settings");
		ImGui::SliderFloat("Orbit radius", &orbitRadius, 1.0f, 10.0f);
		ImGui::SliderFloat("Orbit speed", &orbitSpeed, 1.0f, 10.0f);
		ImGui::SliderFloat("Field of view", &fieldOfView, 60.0f, 110.0f);
		ImGui::SliderFloat("Orthographic height", &orthographicHeight, 1.0f, 100.0f);
		ImGui::Checkbox("Orthographic Enabled", &isOrthographic);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwPollEvents();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

void resizeFrameBufferCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	SCREEN_WIDTH = width;
	SCREEN_HEIGHT = height;
}

void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
	if (keycode == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
}