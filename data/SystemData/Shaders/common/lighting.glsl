// Shared directional lighting term used by the model shaders.
// Half-vector specular is taken against the fixed +Z view direction, matching
// the original fixed-pipeline-era lighting. baseColour is the surface colour the
// light modulates (vertex colour, or vertex colour * texture for the separate-
// specular variant). Passing it as a parameter lets all three model shaders
// share one implementation without changing their results.
vec4 processLight(
    vec4  baseColour,
    vec3  normal,
    vec3  lightDir,
    vec4  lightDiffuseColour,
    vec4  lightSpecularColour,
    vec4  lightAmbientColour,
    float specularAmount,
    float specularExponent)
{
    vec4 colour = lightAmbientColour * baseColour;
    // Diffuse
    LOWP float ndotl = max(0.0, dot(normal, lightDir));
    colour += ndotl * lightDiffuseColour * baseColour;
    // Specular
    LOWP vec3 h_vec = normalize(lightDir + vec3(0.0, 0.0, 1.0));
    LOWP float ndoth = dot(normal, h_vec);
    if (ndoth > 0.0)
    {
        colour += (pow(ndoth, specularExponent) *
            vec4(specularAmount, specularAmount, specularAmount, 1.0) * lightSpecularColour);
    }
    return colour;
}
