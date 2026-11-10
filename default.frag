#version 330 core

out vec4 FragColor;

in vec3 crntPos;
in vec3 Normal;
in vec3 color;
in vec2 texCoord;



uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;
uniform vec3 objectTint;
uniform int useSolidBaseColor;
uniform vec3 solidBaseColor;
uniform vec3 sunDirection;
uniform vec3 ambientColor;


vec4 pointLight()
{

	vec3 lightVec = lightPos - crntPos;


	float dist = length(lightVec);
	float a = 3.0;
	float b = 0.7;
	float inten = 1.0f / (a * dist * dist + b * dist + 1.0f);


	float ambient = 0.20f;


	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightVec);
	float diffuse = max(dot(normal, lightDirection), 0.0f);


	float specularLight = 0.50f;
	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 reflectionDirection = reflect(-lightDirection, normal);
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16.0f);
	float specular = specAmount * specularLight;

	return (texture(diffuse0, texCoord) * (diffuse * inten + ambient) + texture(specular0, texCoord).r * specular * inten) * lightColor;
}

vec4 direcLight()
{

	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(sunDirection);
	float diffuse = max(dot(normal, lightDirection), 0.0f);


	float specularLight = 0.50f;
	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 reflectionDirection = reflect(-lightDirection, normal);
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16.0f);
	float specular = specAmount * specularLight;

	vec3 dayNightLighting = ambientColor + diffuse * max(lightColor.rgb, vec3(0.0));
	return vec4(texture(diffuse0, texCoord).rgb * dayNightLighting + texture(specular0, texCoord).r * specular * max(lightColor.rgb, vec3(0.0)), lightColor.a);
}

vec4 spotLight()
{

	float outerCone = 0.90f;
	float innerCone = 0.95f;


	float ambient = 0.20f;


	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightPos - crntPos);
	float diffuse = max(dot(normal, lightDirection), 0.0f);


	float specularLight = 0.50f;
	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 reflectionDirection = reflect(-lightDirection, normal);
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16.0f);
	float specular = specAmount * specularLight;


	float angle = dot(vec3(0.0f, -1.0f, 0.0f), -lightDirection);
	float inten = clamp((angle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);

	return (texture(diffuse0, texCoord) * (diffuse * inten + ambient) + texture(specular0, texCoord).r * specular * inten) * lightColor;
}


void main()
{
	vec4 litColor = direcLight();
	float solidDiffuse = max(dot(normalize(Normal), normalize(sunDirection)), 0.0);
	vec3 solidLighting = ambientColor + solidDiffuse * max(lightColor.rgb, vec3(0.0));
	vec3 baseColor = useSolidBaseColor != 0 ? solidBaseColor * solidLighting : litColor.rgb;
	FragColor = vec4(baseColor * objectTint, 1.0);
}
