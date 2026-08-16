/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef FRUSTUM_INLINE_HPP
#define FRUSTUM_INLINE_HPP

//=============================================
// @brief
//
//=============================================
inline Float GetXFOVFromY( Float fovY, Float ratio )
{
	Float halfradians = fovY * (0.5f * M_PI / 180.0f);
	Float t = SDL_tan(halfradians) * ratio;

	Float fovX = ((180.0f / M_PI) * SDL_atan(t)) * 2.0f;
	return fovX;
}
#endif //FRUSTUM_INLINE_HPP