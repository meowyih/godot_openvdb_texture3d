shader_type spatial;

render_mode unshaded; // object space
// render_mode unshaded, world_vertex_coords; // world space

uniform sampler3D t3d;
uniform int iteration : hint_range( 8, 56, 1 ) = 32; // total ray move steps
varying vec3 world_camera;
varying vec3 world_position;

float vmax(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}

// imprecise (fast) sdf for box
// c - center, s - size
float sdf_boxcheap(vec3 p, vec3 c, vec3 s)
{
    return vmax(abs(p-c) - s);
}

// get density for specific position p
float density( vec3 p )
{
	p.x = p.x + 0.5;
	p.y = p.y + 0.5;
	p.z = p.z + 0.5;
	// without boundary, Texture3D will tiled repeatly
	if ( p.x < 0. ) return 0.0;
	if ( p.y < 0. ) return 0.0;
	if ( p.z < 0. ) return 0.0;
	if ( p.x > 1. ) return 0.0;
	if ( p.y > 1. ) return 0.0;
	if ( p.z > 1. ) return 0.0;
	
	// NOTE: When I convert openVDB to Texture3D, I store 
	// density attribute to pixel's r', and flames attribute to pixels 'g'.
	// In serious implementation, we might need to store one float value in
	// entire one pixel (24bit) and seperate density and flame into 2 different
	// Texture3D file for more precise result.
	return texture( t3d, p ).r; // r is VDB density
}

void vertex() {
	world_position = VERTEX;
	world_camera = (inverse(MODELVIEW_MATRIX) * vec4(0, 0, 0, 1)).xyz; // object space
	// world_camera = ( INV_VIEW_MATRIX  * vec4(0, 0, 0, 1)).xyz; // world space
}

void fragment() {
	
	// ray start position (ro) and move direction (rd)
	vec3 ro = world_camera;
	vec3 rd =  normalize(world_position - ro);
	
	float density_sum = 0.0; // accumulate density along the ray
	float t = 0.0;
	vec3 pos = ro + rd * t;

	for (int i=0; i<iteration; i++)
    {
		// check ray marching and signed distance field for derail
		float sdf = sdf_boxcheap(pos, vec3(0.0), vec3(0.5));
		
		// summ the total density along the ray
		float density = density(pos);
		density_sum = density_sum + density;
		
		// why not 1.0? To show some detail even the density reach maximum.
		// you can see the different when shading with multiple color
		if ( density_sum >= 0.95 )
			break;
		
		// when iteration=32, 32*0.03=0.96 near 1.0
		// i.e. make sure the ray can scan though the entire box
		t = t + max( sdf, 0.03 );
		pos = ro + rd * t;
	}
	
	ALBEDO = vec3( 1.0 );
	ALPHA = density_sum;
}