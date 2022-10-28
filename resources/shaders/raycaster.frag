#version 450

struct Camera
{
	vec3 pos, forward, right, up;
};

struct Ray
{
	vec3 pos, dir;
};

int eAlphaBlending = 0;
int eFirstHit = 1;
int eMaximumIntensity = 2;
int eFirstLocalMaximum = 3;

vec3 background = vec3( 0.95 );

layout( pixel_center_integer ) in vec4 gl_FragCoord;
uniform vec2 viewport;
uniform Camera camera;

uniform float stepsize;
uniform vec3 clipRegion[2];
uniform vec3 dimensions;
uniform vec3 dimensionScaling;

uniform vec4 shadingParams;
uniform int compositing;

uniform sampler3D gradientVolume;
uniform sampler3D highlightedRegion;
uniform bool showHighlightedRegion;
uniform vec3 highlightedRegionColor;

uniform struct {
	sampler3D mask;
	sampler3D volumes[3];

	sampler1D colorMap1D;
	sampler2D colorMap2D;
	sampler1D colorMap1DAlpha;
	vec2 ranges[3];
	
	bool useColorMap2D;
	bool useColorMap1DAlpha;
} regions[10];
uniform int regionCount;

layout( location = 0 ) out vec4 outColor;

bool getEntryExitPoints( in Ray ray, out vec3 entry, out vec3 exit )
{
	float tEntry = -1.0, tExit = -1.0;

	if( ray.pos.x >= 0.0 && ray.pos.x <= 1.0 && ray.pos.y >= 0.0 && ray.pos.y <= 1.0 && ray.pos.z >= 0.0 && ray.pos.z <= 1.0 )
	{
		tEntry = 0.0;
		entry = ray.pos;
	}

	vec3 l = clipRegion[0];
	vec3 u = clipRegion[1];

	// Check intersection with x = 0
	if( ray.dir.x != 0.0 ) {
		float t = ( l.x - ray.pos.x ) / ray.dir.x;
		if( t >= 0.0 ) {
			vec3 point = ray.pos + t * ray.dir;
			if( point.y >= l.y && point.y <= u.y && point.z >= l.z && point.z <= u.z ) {
				if( tEntry == -1.0 ) {
					tEntry = t;
					entry = point;
				} else {
					if( t < tEntry ) {
						tExit = tEntry;
						exit = entry;
						
						tEntry = t;
						entry = point;
						return true;
					} else {
						tExit = t;
						exit = point;
						return true;
					}
				}
			}
		}
	}

	// Check intersection with y = 0
	if( ray.dir.y != 0.0 ) {
		float t = ( l.y - ray.pos.y ) / ray.dir.y;
		if( t >= 0.0 ) {
			vec3 point = ray.pos + t * ray.dir;
			if( point.x >= l.x && point.x <= u.x && point.z >= l.z && point.z <= u.z ) {
				if( tEntry == -1.0 ) {
					tEntry = t;
					entry = point;
				} else {
					if( t < tEntry ) {
						tExit = tEntry;
						exit = entry;
						
						tEntry = t;
						entry = point;
						return true;
					} else {
						tExit = t;
						exit = point;
						return true;
					}
				}
			}
		}
	}

	// Check intersection with z = 0
	if( ray.dir.z != 0.0 ) {
		float t = ( l.z - ray.pos.z ) / ray.dir.z;
		if( t >= 0.0 ) {
			vec3 point = ray.pos + t * ray.dir;
			if( point.x >= l.x && point.x <= u.x && point.y >= l.y && point.y <= u.y ) {
				if( tEntry == -1.0 ) {
					tEntry = t;
					entry = point;
				} else {
					if( t < tEntry ) {
						tExit = tEntry;
						exit = entry;
						
						tEntry = t;
						entry = point;
						return true;
					} else {
						tExit = t;
						exit = point;
						return true;
					}
				}
			}
		}
	}

	// Check intersection with x = 1
	if( ray.dir.x != 0.0 ) {
		float t = ( u.x - ray.pos.x ) / ray.dir.x;
		if( t >= 0.0 ) {
			vec3 point = ray.pos + t * ray.dir;
			if( point.y >= l.y && point.y <= u.y && point.z >= l.z && point.z <= u.z ) {
				if( tEntry == -1.0 ) {
					tEntry = t;
					entry = point;
				} else {
					if( t < tEntry ) {
						tExit = tEntry;
						exit = entry;
						
						tEntry = t;
						entry = point;
						return true;
					} else {
						tExit = t;
						exit = point;
						return true;
					}
				}
			}
		}
	}

	// Check intersection with y = 1
	if( ray.dir.y != 0.0 ) {
		float t = ( u.y - ray.pos.y ) / ray.dir.y;
		if( t >= 0.0 ) {
			vec3 point = ray.pos + t * ray.dir;
			if( point.x >= l.x && point.x <= u.x && point.z >= l.z && point.z <= u.z ) {
				if( tEntry == -1.0 ) {
					tEntry = t;
					entry = point;
				} else {
					if( t < tEntry ) {
						tExit = tEntry;
						exit = entry;
						
						tEntry = t;
						entry = point;
						return true;
					} else {
						tExit = t;
						exit = point;
						return true;
					}
				}
			}
		}
	}

	// Check intersection with z = 1
	if( ray.dir.z != 0.0 ) {
		float t = ( u.z - ray.pos.z ) / ray.dir.z;
		if( t >= 0.0 ) {
			vec3 point = ray.pos + t * ray.dir;
			if( point.x >= l.x && point.x <= u.x && point.y >= l.y && point.y <= u.y ) {
				if( tEntry == -1.0 ) {
					tEntry = t;
					entry = point;
				} else {
					if( t < tEntry ) {
						tExit = tEntry;
						exit = entry;
						
						tEntry = t;
						entry = point;
						return true;
					} else {
						tExit = t;
						exit = point;
						return true;
					}
				}
			}
		}
	}

	return false;
}

vec4 sampleColor( in vec3 position )
{
	// Scale position based on volumes dimensions to account for non-cubic volume shapes
	position *= dimensionScaling;
	
	// The default return color is fully transparent
	vec4 color = vec4( 0.0 );

	// If region brushing is enabled and the current position is selected, render it
	if( showHighlightedRegion )
	{
		float highlighted = texture( highlightedRegion, position ).x;
		if( highlighted != 0.0 ) {
			color = vec4( highlightedRegionColor, 0.05 );
		}
	}

	// Iterate regions from highest to lowest priority
	if( color.a == 0.0 ) for( int i = 0; i < regionCount; ++i )
	{
		// If region does not contain position, continue
		float maskValue = texture( regions[i].mask, position ).x;
		if( maskValue == 0.0 ) continue;

		// If requested, use 2D color map; otherwise use 1D color map
		if( regions[i].useColorMap2D )
		{
			// Sample and normalize value for the first input volume
			vec4 inVoxel1 = texture( regions[i].volumes[0], position );
			float value1 = ( inVoxel1.x - regions[i].ranges[0].x ) / ( regions[i].ranges[0].y - regions[i].ranges[0].x );
				
			// Sample and normalize value for the second input volume
			vec4 inVoxel2 = texture( regions[i].volumes[1], position );
			float value2 = ( inVoxel2.x - regions[i].ranges[1].x ) / ( regions[i].ranges[1].y - regions[i].ranges[1].x );

			// Sample 2D color using normalized values
			color = texture( regions[i].colorMap2D, vec2( value1, value2 ) );
		} 
		else
		{
			// Sample and normalize value for the input volume
			vec4 inVoxel = texture( regions[i].volumes[0], position );
			float value = ( inVoxel.x - regions[i].ranges[0].x ) / ( regions[i].ranges[0].y - regions[i].ranges[0].x );
			
			// Sample 1D color map using normalized value
			color = texture( regions[i].colorMap1D, value );
		}

		// If requested, use separate 1D color map for alpha value
		if( regions[i].useColorMap1DAlpha )
		{
			// Sample and normalize value for the (alpha) input volume
			vec4 inVoxel = texture( regions[i].volumes[2], position );
			float value = ( inVoxel.x - regions[i].ranges[2].x ) / ( regions[i].ranges[2].y - regions[i].ranges[2].x );

			// Sample (alpha) 1D color map using normalized value
			color.a = texture( regions[i].colorMap1DAlpha, value ).a;
		}

		// If the color is opaque, break
		if( color.a > 0.0 ) break;
	}

	return color;
}
vec3 shading( in Ray ray, in vec3 position, in vec3 color )
{
	vec3 offset = 1.0 / dimensions;
	vec3 gradient = vec3(
		( sampleColor( position + vec3( offset.x, 0.0, 0.0 ) ).a - sampleColor( position - vec3( offset.x, 0.0, 0.0 ) ).a ) / 2.0,
		( sampleColor( position + vec3( 0.0, offset.y, 0.0 ) ).a - sampleColor( position - vec3( 0.0, offset.y, 0.0 ) ).a ) / 2.0,
		( sampleColor( position + vec3( 0.0, 0.0, offset.z ) ).a - sampleColor( position - vec3( 0.0, 0.0, offset.z ) ).a ) / 2.0
	);

	vec3 n = -normalize( gradient );
	vec3 l = normalize( ray.pos - position );
	vec3 r = reflect( ray.dir, n );
	float ambient = shadingParams[0];
	float diffuse = shadingParams[1] * max( 0.0, dot( n, l ) );
	float specular = shadingParams[2] * pow( max( 0.0, dot( r, l ) ), shadingParams[3] );
					
	return ( ambient + diffuse  + specular ) * color.rgb;
}



vec3 raycastAlphaBlending( in Ray ray, in vec3 current, in vec3 dir, in int iterations )
{
	vec3 totalColor = vec3( 0.0 );
	float totalAlpha = 0.0;

	for( int i = 0; i < iterations; ++i )
	{
		// Sample color at position
		vec4 inColor = sampleColor( current );
		float alpha = inColor.a;

		if( alpha > 0.0 )
		{
			// Perform alpha correction
			alpha = 1.0 - pow( 1.0 - alpha, stepsize * 100.0 );

			// Shade color
			vec3 color = shading( ray, current, inColor.rgb ) * alpha;

			// Apply front-to-back composition
			totalColor += ( 1.0 - totalAlpha ) * color;
			totalAlpha += ( 1.0 - totalAlpha ) * alpha;	
			if( totalAlpha > 0.99 ) break;
		}

		current += dir;
	}

	return totalColor + ( 1.0 - totalAlpha ) * background;;
}
vec3 raycastFirstHit( in Ray ray, in vec3 current, in vec3 dir, in int iterations )
{
	vec3 totalColor = background;

	for( int i = 0; i < iterations; ++i )
	{	
		vec4 inColor = sampleColor( current );

		if( inColor.a > 0.0 )
		{
			vec3 color = shading( ray, current, inColor.rgb );

			totalColor = color;
			break;
		}

		current += dir;
	}

	return totalColor;
}
vec3 raycastMaximumIntensity( in Ray ray, in vec3 current, in vec3 dir, in int iterations )
{
	vec3 totalColor = background;
	float maximumAlpha = -1.0;

	for( int i = 0; i < iterations; ++i )
	{
		vec4 inColor = sampleColor( current );

		if( inColor.a > maximumAlpha )
		{
			vec3 color = shading( ray, current, inColor.rgb );

			totalColor = color;
			maximumAlpha = inColor.a;
		}

		current += dir;
	}

	return totalColor;
}
vec3 raycastFirstLocalMaximum( in Ray ray, in vec3 current, in vec3 dir, in int iterations )
{
	vec3 totalColor = background;
	float maximumAlpha = -1.0;

	for( int i = 0; i < iterations; ++i )
	{
		vec4 inColor = sampleColor( current );

		if( inColor.a > maximumAlpha )
		{
			vec3 color = shading( ray, current, inColor.rgb );

			totalColor = color;
			maximumAlpha = inColor.a;
		}
		else break;

		current += dir;
	}

	return totalColor;
}



void main()
{
	// Calculate ray direction
	vec2 weights = ( gl_FragCoord.xy / ( viewport - 1.0 ) ) * 2.0 - 1.0;
	Ray ray = Ray( camera.pos, normalize( ( camera.pos + camera.forward + weights.x * camera.right + weights.y * camera.up ) - camera.pos ) );

	outColor = vec4( background, 1.0 );

	vec3 entry, exit;
	if( getEntryExitPoints( ray, entry, exit ) )
	{
		// Calculate number of iterations and scale direction based on step size
		int iterations = int( ceil( distance( entry, exit ) / stepsize ) );
		vec3 dir = stepsize * ray.dir;

		if( compositing == eAlphaBlending )				outColor.rgb = raycastAlphaBlending( ray, entry, dir, iterations );
		else if( compositing == eFirstHit )				outColor.rgb = raycastFirstHit( ray, entry, dir, iterations );
		else if( compositing == eMaximumIntensity )		outColor.rgb = raycastMaximumIntensity( ray, entry, dir, iterations );
		else if( compositing == eFirstLocalMaximum )	outColor.rgb = raycastFirstLocalMaximum( ray, entry, dir, iterations );
	}
}