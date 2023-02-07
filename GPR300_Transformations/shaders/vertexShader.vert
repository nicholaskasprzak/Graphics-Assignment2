#version 450                          
layout (location = 0) in vec3 vPos;  
layout (location = 1) in vec3 vNormal;

out vec3 Normal;

uniform mat4 transform;
uniform mat4 view;
uniform mat4 projection;

void main(){ 
    Normal = vNormal;
    gl_Position = projection * view * transform * vec4(vPos,1);
}
