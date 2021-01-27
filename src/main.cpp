﻿#include "glew.h"
#include "freeglut.h"
#include "glm.hpp"
#include "ext.hpp"
#include <iostream>
#include <cmath>
#include <ctime>
#include <vector>
#include "Shader_Loader.h"
#include "Render_Utils.h"
#include "Camera.h"
#include "Texture.h"

#include "Box.cpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "model.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
//int winId;
GLuint program;
GLuint programTex;
GLuint programSun;
GLuint programSkybox;
Core::Shader_Loader shaderLoader;

Core::RenderContext armContext;
std::vector<Core::Node> arm;
int ballIndex;

GLuint textureShip_normals;
GLuint sunTexture;
GLuint earthTexture;
GLuint moonTexture;
GLuint skyboxTexture;
GLuint shipTexture;
obj::Model sphereModel;
obj::Model cubeModel;
obj::Model shipModel;

Core::RenderContext sphereContext;
Core::RenderContext cubeContext;
Core::RenderContext shipContext;

//assimp
std::shared_ptr<Model> corvette;
std::vector<Core::RenderContext> corvetteMeshes;

float cameraAngle = 0;
glm::vec3 cameraPos = glm::vec3(-6, 0, 0);
glm::vec3 cameraDir;
glm::vec3 cameraSide;


glm::mat4 cameraMatrix, perspectiveMatrix;

glm::vec3 sunPos = glm::vec3(10.0f, 0.0f, -5.0f);
glm::vec3 sunPos2 = glm::vec3(25.0f, -1.0f, 10.0f);


struct Light {
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
};

int engineLightTimer;

//wczytywanie skyboxa (musi byc jpg!)
std::vector<std::string> faces
{
	"skybox/right.jpg",
	"skybox/left.jpg",
	"skybox/top.jpg",
	"skybox/bottom.jpg",
	"skybox/front.jpg",
	"skybox/back.jpg"
};


std::vector<Light> lights;

void keyboard(unsigned char key, int x, int y)
{
	float angleSpeed = 0.1f;
	float moveSpeed = 0.1f;
	switch (key)
	{
	case 'q': cameraAngle -= angleSpeed; break;
	case 'e': cameraAngle += angleSpeed; break;
	case 'w': 
	{
		cameraPos += cameraDir * moveSpeed;
		lights[2].intensity = 0.001;
		lights[3].intensity = 0.001;
		engineLightTimer = 0;
		break;
	}
	case 's': cameraPos -= cameraDir * moveSpeed; break;
	case 'd': cameraPos += glm::cross(cameraDir, glm::vec3(0, 1, 0)) * moveSpeed; break;
	case 'a': cameraPos -= glm::cross(cameraDir, glm::vec3(0, 1, 0)) * moveSpeed; break;
	case 'z': cameraPos += glm::cross(cameraDir, glm::vec3(0, 0, 1)) * moveSpeed; break;
	case 'x': cameraPos -= glm::cross(cameraDir, glm::vec3(0, 0, 1)) * moveSpeed; break;
	}
}

glm::mat4 createCameraMatrix()
{
	// Obliczanie kierunku patrzenia kamery (w plaszczyznie x-z) przy uzyciu zmiennej cameraAngle kontrolowanej przez klawisze.
	cameraDir = glm::vec3(cosf(cameraAngle), 0.0f, sinf(cameraAngle));
	glm::vec3 up = glm::vec3(0, 1, 0);

	cameraSide = glm::cross(cameraDir,up);

	return Core::createViewMatrix(cameraPos, cameraDir, up);
}
float frustumScale = 1.f;



void drawObject(GLuint program, Core::RenderContext context, glm::mat4 modelMatrix, glm::vec3 color)
{
	glUseProgram(program);

	glUniform3f(glGetUniformLocation(program, "objectColor"), color.x, color.y, color.z);

	glm::mat4 transformation = perspectiveMatrix * cameraMatrix * modelMatrix;


	glUniformMatrix4fv(glGetUniformLocation(program, "modelMatrix"), 1, GL_FALSE, (float*)&modelMatrix);
	glUniformMatrix4fv(glGetUniformLocation(program, "transformation"), 1, GL_FALSE, (float*)&transformation);

	Core::DrawContext(context);
	glUseProgram(0);
}

//funkcja rysujaca modele za pomoca assimpa
void drawFromAssimpModel(GLuint program, std::shared_ptr<Model> model, glm::mat4 modelMatrix, glm::vec3 color)
{
	glUseProgram(program);

	glUniform3f(glGetUniformLocation(program, "objectColor"), color.x, color.y, color.z);

	glm::mat4 transformation = perspectiveMatrix * cameraMatrix * modelMatrix;

	glUniformMatrix4fv(glGetUniformLocation(program, "modelMatrix"), 1, GL_FALSE, (float*)&modelMatrix);
	glUniformMatrix4fv(glGetUniformLocation(program, "transformation"), 1, GL_FALSE, (float*)&transformation);

	model->Draw(program);

	glUseProgram(0);
}

//Skybox
unsigned int loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}
void drawSkybox(GLuint program, Core::RenderContext context, GLuint texID)
{
	glUseProgram(program);
	glm::mat4 transformation = perspectiveMatrix * glm::mat4(glm::mat3(cameraMatrix));
	glUniformMatrix4fv(glGetUniformLocation(program, "transformation"), 1, GL_FALSE, (float*)&transformation);
	glDepthMask(GL_FALSE);
	//Core::SetActiveTexture(texID, "skybox", program, 0);
	glUniform1i(glGetUniformLocation(program, "skybox"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texID);
	Core::DrawContext(context);
	glDepthMask(GL_TRUE);
	glUseProgram(0);
}

//Textures
void drawObjectTexture(GLuint program, Core::RenderContext context, glm::mat4 modelMatrix, glm::vec3 texture, GLuint texID)
{
	glUseProgram(program);
	glm::mat4 transformation = perspectiveMatrix * cameraMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(program, "modelMatrix"), 1, GL_FALSE, (float*)&modelMatrix);
	glUniform3f(glGetUniformLocation(program, "colorTex"), texture.x, texture.y, texture.z);
	glUniformMatrix4fv(glGetUniformLocation(program, "transformation"), 1, GL_FALSE, (float*)&transformation);

	Core::SetActiveTexture(texID, "colorTexture", program, 0);

	Core::DrawContext(context);
	glUseProgram(0);
}

//funkcja rysujaca planety (bez obracania wokol wlasnej osi bo ksiezyce sie psuja)
glm::mat4 drawPlanet(float time, glm::vec3 sunPos, glm::vec3 orbit, glm::vec3 translation, glm::vec3 scale)
{
	glm::mat4 planetModelMatrix = glm::mat4(1.0f);
	planetModelMatrix = glm::translate(planetModelMatrix, sunPos); 
	planetModelMatrix = glm::rotate(planetModelMatrix, time, orbit);
	planetModelMatrix = glm::translate(planetModelMatrix, translation);
	planetModelMatrix = glm::scale(planetModelMatrix, scale);
	return planetModelMatrix;
}

//funkcja rysujaca ksiezyce orbitujace wokol danej planety
glm::mat4 drawMoon(glm::mat4 planetModelMatrix, float time, glm::vec3 orbit, glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
	glm::mat4 moonModelMatrix = glm::mat4(planetModelMatrix);
	moonModelMatrix = glm::rotate(moonModelMatrix, time, orbit);
	moonModelMatrix = glm::translate(moonModelMatrix, translation);
	moonModelMatrix = glm::rotate(moonModelMatrix, time, rotation);
	moonModelMatrix = glm::scale(moonModelMatrix, scale);
	return moonModelMatrix;
}

void renderScene()
{
	cameraMatrix = createCameraMatrix();
	perspectiveMatrix = Core::createPerspectiveMatrix(0.01f, 1000.0f, frustumScale);
	float time = glutGet(GLUT_ELAPSED_TIME) / 1000.f;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	drawSkybox(programSkybox, cubeContext, skyboxTexture);
	
	glUseProgram(programSun);
	glUniform3f(glGetUniformLocation(programSun, "cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);

	//ustalanie pozycji s³oñc (lightPos)


	//rysowanie s³oñc
	glm::mat4 sunModelMatrix = glm::mat4(1.0f);
	sunModelMatrix = glm::translate(sunModelMatrix, sunPos);
	sunModelMatrix = glm::scale(sunModelMatrix, glm::vec3(3.0f, 3.0f, 3.0f));
	drawObjectTexture(programSun, sphereContext, sunModelMatrix, glm::vec3(1.5f, 1.8f, 1.8f), sunTexture);

	glm::mat4 sunModelMatrix2 = glm::mat4(1.0f);
	sunModelMatrix2 = glm::translate(sunModelMatrix2, sunPos2);
	drawObjectTexture(programSun, sphereContext, sunModelMatrix2, glm::vec3(0.9f, 0.9f, 2.0f), sunTexture);

	glUseProgram(programTex);



	lights[0].position = sunPos;
	lights[1].position = sunPos2;

	glm::mat4 shipModelMatrix = glm::translate(cameraPos + cameraDir * 0.6f + glm::vec3(0, -0.25f, 0)) * glm::rotate(-cameraAngle + glm::radians(90.0f), glm::vec3(0, 1, 0)) * glm::scale(glm::vec3(0.0001f));
	glm::mat4 engineLeft = glm::translate(shipModelMatrix, glm::vec3(500,0,-1500));
	lights[2].position = glm::vec3(engineLeft[3][0], engineLeft[3][1], engineLeft[3][2]);

	glm::mat4 engineRight = glm::translate(shipModelMatrix, glm::vec3(-500, 0, -1500));
	lights[3].position = glm::vec3(engineRight[3][0], engineRight[3][1], engineRight[3][2]);

	
	for (int i = 0; i < lights.size(); i++)
	{
		std::string col = "pointLights[" + std::to_string(i) + "].color";
		std::string pos = "pointLights[" + std::to_string(i) + "].position";
		std::string ins = "pointLights[" + std::to_string(i) + "].intensity";
		glUniform3f(glGetUniformLocation(programTex, col.c_str()), lights[i].color.x, lights[i].color.y, lights[i].color.z);
		glUniform3f(glGetUniformLocation(programTex, pos.c_str()), lights[i].position.x, lights[i].position.y, lights[i].position.z);
		glUniform1f(glGetUniformLocation(programTex, ins.c_str()), lights[i].intensity);
	}

	glUniform3f(glGetUniformLocation(programTex, "cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);


	//rysowanie statku
	//glm::mat4 shipModelMatrix = glm::translate(cameraPos + cameraDir * 0.6f + glm::vec3(0, -0.25f, 0)) * glm::rotate(-cameraAngle + glm::radians(90.0f), glm::vec3(0, 1, 0)) * glm::scale(glm::vec3(0.0001f));

	drawFromAssimpModel(programTex, corvette, shipModelMatrix, glm::vec3(1));




	//rysowanie Ziemi z ksiê¿ycem
	glm::mat4 earth = drawPlanet(time / 5.0f, sunPos*glm::vec3(1.5f,1,1), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-10.5f, 0.0f, -10.5f), glm::vec3(0.5f, 0.5f, 0.5f));
	glm::mat4 moon = drawMoon(earth, time/2.0f, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0, 1, 1), glm::vec3(1.5f, 1.0f, 1.0f), glm::vec3(0.3f, 0.3f, 0.3f));
	earth = glm::rotate(earth, time/3.0f, glm::vec3(0.0f, 0.0f, 1.0f));
	drawObjectTexture(programTex, sphereContext, earth, glm::vec3(0.8f, 0.8f, 0.8f), earthTexture);
	drawObjectTexture(programTex, sphereContext, moon, glm::vec3(0.9f, 1.0f, 0.9f), moonTexture);

	if (engineLightTimer < 50) engineLightTimer++;
	else
	{
		lights[2].intensity = 0.00001;
		lights[3].intensity = 0.00001;
	}

	glUseProgram(0);
	glutSwapBuffers();
}

void init()
{
	glEnable(GL_DEPTH_TEST);
	program = shaderLoader.CreateProgram("shaders/shader_4_1.vert", "shaders/shader_4_1.frag");
	programTex = shaderLoader.CreateProgram("shaders/shader_4_tex.vert", "shaders/shader_4_tex.frag");
	programSun = shaderLoader.CreateProgram("shaders/shader_4_sun.vert", "shaders/shader_4_sun.frag");
	programSkybox = shaderLoader.CreateProgram("shaders/shader_skybox.vert", "shaders/shader_skybox.frag");

	

	
	corvette = std::make_shared<Model>("models/Corvette-F3.obj");
	//shipModel = obj::loadModelFromFile("models/spaceship.obj");
	sphereModel = obj::loadModelFromFile("models/sphere.obj");
	cubeModel = obj::loadModelFromFile("models/cube.obj");

	sphereContext.initFromOBJ(sphereModel);
	cubeContext.initFromOBJ(cubeModel);
	//shipContext.initFromOBJ(shipModel);
	shipTexture = Core::LoadTexture("textures/spaceship.png");
	sunTexture = Core::LoadTexture("textures/sun.png");
	earthTexture = Core::LoadTexture("textures/earth2.png");
	moonTexture = Core::LoadTexture("textures/moon.png");
	skyboxTexture = loadCubemap(faces);
	
	Light l1;
	l1.position = sunPos;
	l1.color = glm::vec3(0.8f, 0.8f, 0.7f);
	l1.intensity = 2;
	lights.push_back(l1);

	Light l2;
	l2.position = sunPos2;
	l2.color = glm::vec3(0.5f, 0.5f, 0.5f);
	l2.intensity = 2;
	lights.push_back(l2);

	Light l3;
	l3.position = glm::vec3(0);
	l3.color = glm::vec3(1.0f, 0.0f, 0.0f);
	l3.intensity = 0.0001;
	lights.push_back(l3);

	Light l4;
	l4.position = glm::vec3(0);
	l4.color = glm::vec3(1.0f, 0.0f, 0.0f);
	l4.intensity = 0.0001;
	lights.push_back(l4);


}

void shutdown()
{
	shaderLoader.DeleteProgram(program);
}

void onReshape(int width, int height)
{
	// Kiedy rozmiar okna sie zmieni, obraz jest znieksztalcony.
	// Dostosuj odpowiednio macierz perspektywy i viewport.
	// Oblicz odpowiednio globalna zmienna "frustumScale".
	// Ustaw odpowiednio viewport - zobacz:
	// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glViewport.xhtml
	frustumScale = (float)width / (float)height;
	glViewport(0, 0, width, height);
}
void idle()
{
	glutPostRedisplay();
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glEnable(GL_MULTISAMPLE);
	glutInitWindowPosition(100, 200);
	glutInitWindowSize(1240, 720);
	glutCreateWindow("GRK-PROJECT WIP");
	//winId = glutCreateWindow("OpenGL + PhysX");
	//glutFullScreen();
	glewInit();

	init();
	glutKeyboardFunc(keyboard);

	//to sprawia, że obiekty ukryte przed kamerą  nie są renderowane
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	glutDisplayFunc(renderScene);
	glutIdleFunc(idle);

	glutReshapeFunc(onReshape);
	glutMainLoop();

	shutdown();

	return 0;
}
