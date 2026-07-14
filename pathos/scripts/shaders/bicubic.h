// Bicubic lightmapping code based on godot engine implementation
float w0(float a) 
{
	return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a) 
{
	return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a) 
{
	return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a) 
{
	return (1.0 / 6.0) * (a * a * a);
}

float g0(float a) 
{
	return w0(a) + w1(a);
}

float g1(float a) 
{
	return w2(a) + w3(a);
}

float h0(float a) 
{
	return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a) 
{
	return 1.0 + w3(a) / (w2(a) + w3(a));
}