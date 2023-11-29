cbuffer PerFrameCB {
    uint2 gFrameDim;
    float gAngleThreshold;
    uint mDepthScale;
    float tan2FovX;
    float tan2FovY;
    int gVisibilityKernelSize;
};

Texture2D<uint> gDepthTex;

RWTexture2D<uint> gOutputDepthTex;

float3 calCameraPos(uint2 coord, float linearZ) {

    float2 Crd = (coord + 0.5f) / gFrameDim;

    Crd -= 0.5f;

#ifdef FALCOR_FLIP_Y
    Crd *= float2(2., 2.);
#else
    Crd *= float2(2., -2.);
#endif

    float3 posC;
    posC.x = Crd.x * linearZ * tan2FovX;
    posC.y = Crd.y * linearZ * tan2FovY;
    posC.z = linearZ;

    return posC;
}

int getSectorIndex(int2 dCoord)
{
    if (dCoord.x <= 0) {

        if (dCoord.y < 0) {
            if (abs(dCoord.x) < abs(dCoord.y)) {
                return 0;
            }
            else {
                return 1;
            }
        }
        else {
            if (abs(dCoord.x) > abs(dCoord.y)) {
                return 2;
            }
            else {
                return 3;
            }
        }

    }
    else {

            if (dCoord.y > 0) {
                if (abs(dCoord.x) <= abs(dCoord.y)) {
                    return 4;
                }
                else {
                    return 5;
                }
            }
            else {
                if (abs(dCoord.x) > abs(dCoord.y)) {
                    return 6;
                }
                else {
                    return 7;
                }
            }
    }
}

[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {


    uint2 pixelCoord = dispatchThreadId.xy;


    if (gDepthTex[pixelCoord] == uint(-1)) {
        gOutputDepthTex[pixelCoord] = uint(-1);
        return;
    }

    float maxSectorAnglesCos[8] = {-1, -1, -1, -1, -1, -1, -1, -1};


    float3 centerPosC = calCameraPos(pixelCoord, (float)gDepthTex[pixelCoord] / mDepthScale);
    float3 cameraDir = -normalize(centerPosC);

    for (int dx = -gVisibilityKernelSize; dx <= gVisibilityKernelSize; dx++) {
        for (int dy = -gVisibilityKernelSize; dy <= gVisibilityKernelSize; dy++) {

            if (dx == 0 && dy == 0) {
                continue;
            }

            int2 NeighborCoord = pixelCoord + int2(dx, dy);

            if (NeighborCoord.x < 0 || NeighborCoord.x >= gFrameDim.x || NeighborCoord.y < 0 || NeighborCoord.y >= gFrameDim.y || gDepthTex[NeighborCoord] == uint(-1)) {
                continue;
            }

            int2 dCoord = int2(dx, dy);

            int sectorIndex = getSectorIndex(dCoord);

            float3 neighborPosC = calCameraPos(NeighborCoord, (float)gDepthTex[NeighborCoord] / mDepthScale);

            float3 dir = normalize(neighborPosC - centerPosC);

            float angleCos = dot(dir, cameraDir);

            maxSectorAnglesCos[sectorIndex] = max(maxSectorAnglesCos[sectorIndex], angleCos);

        }
    }

    float angleSum = 0;
    for (int i = 0; i < 8; i++) {
        angleSum += acos(maxSectorAnglesCos[i]);
    }

    angleSum /= 8.;

    if (angleSum < gAngleThreshold) {
        gOutputDepthTex[pixelCoord] = uint(-1);
    }
    else {
        gOutputDepthTex[pixelCoord] = gDepthTex[pixelCoord];
    }


    return;

}
