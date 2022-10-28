#version 450

layout( location = 0 ) flat in int shouldDiscard;
layout( location = 1 ) in vec2 textureCoords;

uniform sampler1D colorMap1D;
uniform sampler2D colorMap2D;
uniform bool useColorMap1D;
uniform bool useColorMap2D;

uniform vec4 inColor;

layout( location = 0 ) out vec4 outColor;

void main()
{
	if( shouldDiscard == 1 ) discard;

	if( useColorMap1D ) outColor = texture( colorMap1D, textureCoords.x );
	else if( useColorMap2D ) outColor = texture( colorMap2D, textureCoords );
	else outColor = inColor;
}