#version 450

struct Axis
{
	vec2 range;
	int volume;
	float x;
};

layout( binding = 0 ) restrict readonly buffer BufferValues
{
	float values[];
};
layout( binding = 1 ) restrict readonly buffer BufferVisibility
{
	uint visibility[];
};
layout( binding = 2 ) restrict readonly buffer BufferSelection
{
	float selection[];
};
layout( binding = 3 ) restrict readonly buffer BufferPermutation
{
	int permutation[];
};
uniform int voxelCount;
uniform float requiredSelection;

uniform vec2 colorMapRanges[2];
uniform int colorMapVolumeIndices[2];
uniform bool useColorMap1D;
uniform bool useColorMap2D;

uniform Axis axes[32];

layout( location = 0 ) flat out int shouldDiscard;
layout( location = 1 ) out vec2 textureCoords;

void main()
{
	// Get the vertical position of the sample
	Axis axis = axes[gl_VertexID];
	vec2 range = axis.range;

	int sampleIndex = permutation[gl_InstanceID];
	float value = values[axis.volume * voxelCount + sampleIndex];
	float y = clamp( ( value - range.x ) / ( range.y - range.x ) * 2.0 - 1.0, -0.995, 0.995 );

	// Check if this sample should not be rendered
	shouldDiscard = int( visibility[sampleIndex] == 0 || selection[sampleIndex] != requiredSelection );

	// Get color map texture coordinates
	if( useColorMap1D ) 
	{
		value = values[axes[colorMapVolumeIndices[0]].volume * voxelCount + sampleIndex];
		textureCoords.x = ( value - colorMapRanges[0].x ) / ( colorMapRanges[0].y - colorMapRanges[0].x );
	} 
	else if( useColorMap2D ) 
	{
		float first = values[axes[colorMapVolumeIndices[0]].volume * voxelCount + sampleIndex];
		float second = values[axes[colorMapVolumeIndices[1]].volume * voxelCount + sampleIndex];
		textureCoords.x = ( first - colorMapRanges[0].x ) / ( colorMapRanges[0].y - colorMapRanges[0].x );
		textureCoords.y = ( second - colorMapRanges[1].x ) / ( colorMapRanges[1].y - colorMapRanges[1].x );
	}
	
	gl_Position = vec4( axis.x, y, 0.0, 1.0 );
}