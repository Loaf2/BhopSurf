#include "Camera.h"

Camera::Camera(int width, int height, glm::vec3 position)
{
	Camera::width = width;
	Camera::height = height;
	Position = position;
}

void Camera::updateMatrix(float FOVdeg, float nearPlane, float farPlane)
{
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);

	view = glm::lookAt(Position, Position + Orientation, Up);
	projection = glm::perspective(glm::radians(FOVdeg), (float)width / height, nearPlane, farPlane);

	cameraMatrix = projection * view;
}

void Camera::Matrix(Shader& shader, const char* uniform)
{
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, uniform), 1, GL_FALSE, glm::value_ptr(cameraMatrix));
}



void Camera::Inputs(bool allowMovement, bool allowLook)
{
	if (allowMovement)
	{
		if (platform::isKeyDown(platform::Key::LeftShift))
			speed = 42.0f;
		else
			speed = 12.0f;
	}

	double mouseX = 0.0;
	double mouseY = 0.0;
	platform::getLookPosition(mouseX, mouseY);

	if (!allowLook || !mouseInitialized)
	{
		lastMouseX = mouseX;
		lastMouseY = mouseY;
		mouseInitialized = true;
		return;
	}

	float offsetX = static_cast<float>(mouseX - lastMouseX) * sensitivity;
	float offsetY = static_cast<float>(mouseY - lastMouseY) * sensitivity;
	lastMouseX = mouseX;
	lastMouseY = mouseY;

	yaw += offsetX;
	pitch -= offsetY;
	pitch = glm::clamp(pitch, -89.0f, 89.0f);

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	Orientation = glm::normalize(direction);

}
