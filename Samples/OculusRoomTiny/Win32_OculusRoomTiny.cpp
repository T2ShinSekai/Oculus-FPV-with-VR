/************************************************************************************

Filename    :   Win32_OculusRoomTiny2.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse, Tom Heath, Volga Aksoy

Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

//-------------------------------------------------------------------------------------
// This app renders a simple flat-shaded room allowing the user to move along the 
// floor and look around with an HMD and mouse/keyboard. 
// The following keys work:
//  'W', 'S', 'A', 'D', 'F' - Move forward, back; strafe left/right, toggle freeze in timewarp.
// The world right handed coordinate system is defined as  Y -> Up, Z -> Back, X -> Right

// Include the OculusVR SDK
#include "OVR_CAPI.h"

// Choose whether the SDK performs rendering/distortion, or the application. 
#define          SDK_RENDER 0  //Do NOT switch until you have viewed and understood the Health and Safety message.
                               //Disabling this makes it a non-compliant app, and not suitable for demonstration. In place for development only.
const bool       FullScreen = true; //Should be true for correct timing.  Use false for debug only.
#define          VIDEO_CAPTURE  1
#define			 SERVE_CTL	1

// Include Non-SDK supporting Utilities from other files
#include "RenderTiny_D3D11_Device.h"
HWND Util_InitWindowAndGraphics    (Recti vp, int fullscreen, int multiSampleCount, bool UseAppWindowFrame, RenderDevice ** pDevice);
void Util_ReleaseWindowAndGraphics (RenderDevice* pRender);
bool Util_RespondToControls        (float & EyeYaw, Vector3f & EyePos, Quatf PoseOrientation);
void PopulateRoomScene             (Scene* scene, RenderDevice* render);

//Structures for the application
ovrHmd             HMD;
ovrEyeRenderDesc   EyeRenderDesc[2];
ovrRecti           EyeRenderViewport[2];
RenderDevice*      pRender = 0;
Texture*           pRendertargetTexture = 0;
Scene*             pRoomScene = 0;

#if VIDEO_CAPTURE
typedef struct _VideoCapVertex {
	float position[3];
	float texcoord[2];
	float color[4];
} VideoCapVertex;
Texture*			pVideoCaptureTexture = 0;
ShaderSet *			VideoCapShaders;
ID3D11InputLayout * VideoCapVertexIL;
Ptr<Buffer>			VideoCapVB;
Ptr<Buffer>			VideoCapIB;
//VideoCaptureDevice	VideoCapturingDevice;
cv::VideoCapture	Capture1(0);
cv::VideoCapture	Capture2(1);
cv::Mat				CvMat1;
cv::Mat				CvMat2;
cv::Mat				InputMat1;
cv::Mat				InputMat2;
XbeeController		SerialPort;
#endif

// Specifics for whether the SDK or the APP is doing the distortion.
#if SDK_RENDER
	#define OVR_D3D_VERSION 11
	#include "OVR_CAPI_D3D.h"
	ovrD3D11Texture    EyeTexture[2];
#else
	ShaderSet *         Shaders;  
	ID3D11InputLayout * VertexIL;
	Ptr<Buffer>         MeshVBs[2];
	Ptr<Buffer>         MeshIBs[2]; 
	ovrVector2f         UVScaleOffset[2][2];
#endif


   
//-------------------------------------------------------------------------------------
int Init()
{
    // Initializes LibOVR, and the Rift
    ovr_Initialize();
	HMD = ovrHmd_Create(0);
	if (!HMD)
    {
        MessageBoxA(NULL,"Oculus Rift not detected.","", MB_OK);
        return(1);
    }
	if (HMD->ProductName[0] == '\0') 
        MessageBoxA(NULL,"Rift detected, display not enabled.","", MB_OK);

	//Setup Window and Graphics - use window frame if relying on Oculus driver
	const int backBufferMultisample = 1;
    bool UseAppWindowFrame = (HMD->HmdCaps & ovrHmdCap_ExtendDesktop) ? false : true;
    HWND window = Util_InitWindowAndGraphics(Recti(HMD->WindowsPos, HMD->Resolution),
                                         FullScreen, backBufferMultisample, UseAppWindowFrame, &pRender);
	if (!window) return 1;
	ovrHmd_AttachToWindow(HMD, window, NULL, NULL);

    //Configure Stereo settings.
    Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(HMD, ovrEye_Left,  HMD->DefaultEyeFov[0], 1.0f);
    Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(HMD, ovrEye_Right, HMD->DefaultEyeFov[1], 1.0f);
	Sizei RenderTargetSize;
    RenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
    RenderTargetSize.h = max( recommenedTex0Size.h, recommenedTex1Size.h );

    const int eyeRenderMultisample = 1;
    pRendertargetTexture = pRender->CreateTexture(Texture_RGBA | Texture_RenderTarget |
                                                  eyeRenderMultisample,
                                                  RenderTargetSize.w, RenderTargetSize.h, NULL);
    // The actual RT size may be different due to HW limits.
    RenderTargetSize.w = pRendertargetTexture->GetWidth();
    RenderTargetSize.h = pRendertargetTexture->GetHeight();

	// Video Capture Setup
	#if VIDEO_CAPTURE
	// Shader vertex format
	D3D11_INPUT_ELEMENT_DESC VideoCaptureVertexDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }};

	// Video Capture vertex shader
	const char* BackGroundVertexShaderSrc =
		"struct VS_INPUT {\n"
		"	float4 m_vPosition : POSITION;\n"
		"	float2 m_vTexcoord : TEXCOORD;\n"
		"	float4 m_vColor    : COLOR;\n"
		"};\n"
		"struct VS_OUTPUT {\n"
		"	float4 m_vPosition : SV_POSITION;\n"
		"	float2 m_vTexcoord : TEXCOORD;\n"
		"	float4 m_vColor    : COLOR;\n"
		"};\n"
		"float4x4 Projection;\n"
		"VS_OUTPUT main(VS_INPUT Input) {\n"
		"	VS_OUTPUT Output;\n"
		"	Output.m_vPosition = mul(Projection, Input.m_vPosition);\n"
		"	Output.m_vTexcoord = Input.m_vTexcoord;\n"
		"	Output.m_vColor = Input.m_vColor;\n"
		"	return Output;\n"
		"}\n";

	// Video Capture pixel shader
	static const char* BackGroundPixelShaderSrc =
		"struct VS_OUTPUT {\n"
		"	float4 m_vPosition : SV_POSITION;\n"
		"	float2 m_vTexcoord : TEXCOORD;\n"
		"	float4 m_vColor    : COLOR;\n"
		"};\n"
		"Texture2D			g_txDiffuse : register(t0);\n"
		"SamplerState		g_samLinear : register(s0);\n"
		"float4 main(VS_OUTPUT Input) : SV_TARGET{\n"
		"	return g_txDiffuse.Sample(g_samLinear, Input.m_vTexcoord);\n"
		"}\n";

	pRender->InitShaders(BackGroundVertexShaderSrc, BackGroundPixelShaderSrc, &VideoCapShaders, &VideoCapVertexIL, VideoCaptureVertexDesc, 3);

	pVideoCaptureTexture = pRender->CreateTexture(Texture_RGBA | Texture_GenMipmaps, 720, 480, NULL);

	const VideoCapVertex CapVertex[] = {
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ {  1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ { -1.0f,-1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ { -1.0f,-1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ {  1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ {  1.0f,-1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } } };
	VideoCapVB = *pRender->CreateBuffer();
	VideoCapVB->Data(Buffer_Vertex, CapVertex, sizeof(VideoCapVertex) * 6);
	
	const uint16_t CubeIndices[] =
		{	0, 1, 3,
			1, 2, 3,
			1, 5, 2,  };
	VideoCapIB = *pRender->CreateBuffer();
	VideoCapIB->Data(Buffer_Index, CubeIndices, sizeof(CubeIndices));
	
	Capture1.set(CV_CAP_PROP_FRAME_WIDTH,  360); 
	Capture1.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
	Capture2.set(CV_CAP_PROP_FRAME_WIDTH,  360);
	Capture2.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
	#endif

	// initialize com port
	#if SERVE_CTL
	SerialPort.SelectComPort();
	XbeeController::COM_PORT_DESC ComDesc;
	memset(&ComDesc, 0, sizeof(ComDesc));
	ComDesc.BaudRate = 9600;
	memcpy(ComDesc.XbeeName, "xbeeZB", sizeof(ComDesc.XbeeName));
	memcpy(ComDesc.PortName, SerialPort.getPort(), sizeof(ComDesc.PortName));
	if (!SerialPort.OpenPort(&ComDesc))
		MessageBoxA(NULL, "Serial Port is failed to open.", "", MB_OK);
	#endif

    // Initialize eye rendering information.
    // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
    ovrFovPort eyeFov[2] = { HMD->DefaultEyeFov[0], HMD->DefaultEyeFov[1] } ;

    EyeRenderViewport[0].Pos  = Vector2i(0,0);
    EyeRenderViewport[0].Size = Sizei(RenderTargetSize.w / 2, RenderTargetSize.h);
    EyeRenderViewport[1].Pos  = Vector2i((RenderTargetSize.w + 1) / 2, 0);
    EyeRenderViewport[1].Size = EyeRenderViewport[0].Size;

    #if SDK_RENDER
	// Query D3D texture data.
    EyeTexture[0].D3D11.Header.API            = ovrRenderAPI_D3D11;
    EyeTexture[0].D3D11.Header.TextureSize    = RenderTargetSize;
    EyeTexture[0].D3D11.Header.RenderViewport = EyeRenderViewport[0];
    EyeTexture[0].D3D11.pTexture              = pRendertargetTexture->Tex.GetPtr();
    EyeTexture[0].D3D11.pSRView               = pRendertargetTexture->TexSv.GetPtr();

    // Right eye uses the same texture, but different rendering viewport.
    EyeTexture[1] = EyeTexture[0];
    EyeTexture[1].D3D11.Header.RenderViewport = EyeRenderViewport[1];

    // Configure d3d11.
    ovrD3D11Config d3d11cfg;
    d3d11cfg.D3D11.Header.API         = ovrRenderAPI_D3D11;
    d3d11cfg.D3D11.Header.RTSize      = Sizei(HMD->Resolution.w, HMD->Resolution.h);
    d3d11cfg.D3D11.Header.Multisample = backBufferMultisample;
    d3d11cfg.D3D11.pDevice            = pRender->Device;
    d3d11cfg.D3D11.pDeviceContext     = pRender->Context;
    d3d11cfg.D3D11.pBackBufferRT      = pRender->BackBufferRT;
    d3d11cfg.D3D11.pSwapChain         = pRender->SwapChain;

    if (!ovrHmd_ConfigureRendering(HMD, &d3d11cfg.Config,
		                           ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
                                   ovrDistortionCap_TimeWarp  | ovrDistortionCap_Overdrive,
								   eyeFov, EyeRenderDesc))	return(1);
    #else
	//Shader vertex format
	D3D11_INPUT_ELEMENT_DESC DistortionMeshVertexDesc[] = {
		{"Position", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"Position", 1, DXGI_FORMAT_R32_FLOAT,      0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"Position", 2, DXGI_FORMAT_R32_FLOAT,      0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT,   0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT,   0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}};
	
	//Distortion vertex shader
	const char* vertexShader = 
		"float2 EyeToSourceUVScale, EyeToSourceUVOffset;                                        \n"
		"float4x4 EyeRotationStart, EyeRotationEnd;                                             \n"
		"float2 TimewarpTexCoord(float2 TexCoord, float4x4 rotMat)                              \n"
		"{                                                                                      \n"
		// Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic 
		// aberration and distortion). These are now "real world" vectors in direction (x,y,1) 
		// relative to the eye of the HMD.	Apply the 3x3 timewarp rotation to these vectors.
		"    float3 transformed = float3( mul ( rotMat, float4(TexCoord.xy, 1, 1) ).xyz);       \n"
		// Project them back onto the Z=1 plane of the rendered images.
		"    float2 flattened = (transformed.xy / transformed.z);                               \n"
		// Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
		"    return(EyeToSourceUVScale * flattened + EyeToSourceUVOffset);                      \n"
		"}                                                                                      \n"
		"void main(in float2  Position   : POSITION,  in float timewarpLerpFactor : POSITION1,  \n"
		"          in float   Vignette   : POSITION2, in float2 TexCoord0         : TEXCOORD0,  \n"
		"          in float2  TexCoord1  : TEXCOORD1, in float2 TexCoord2         : TEXCOORD2,  \n"
		"          out float4 oPosition  : SV_Position,                                         \n"
		"          out float2 oTexCoord0 : TEXCOORD0, out float2 oTexCoord1 : TEXCOORD1,        \n"
		"          out float2 oTexCoord2 : TEXCOORD2, out float  oVignette  : TEXCOORD3)        \n"
		"{                                                                                      \n"
		"    float4x4 lerpedEyeRot = lerp(EyeRotationStart, EyeRotationEnd, timewarpLerpFactor);\n"
		"    oTexCoord0  = TimewarpTexCoord(TexCoord0,lerpedEyeRot);                            \n"
		"    oTexCoord1  = TimewarpTexCoord(TexCoord1,lerpedEyeRot);                            \n"
		"    oTexCoord2  = TimewarpTexCoord(TexCoord2,lerpedEyeRot);                            \n"
		"    oPosition = float4(Position.xy, 0.5, 1.0);    oVignette = Vignette;                \n"
		"}";

	//Distortion pixel shader
	const char* pixelShader = 
		"Texture2D Texture   : register(t0);                                                    \n"
		"SamplerState Linear : register(s0);                                                    \n"
		"float4 main(in float4 oPosition  : SV_Position,  in float2 oTexCoord0 : TEXCOORD0,     \n"
		"            in float2 oTexCoord1 : TEXCOORD1,    in float2 oTexCoord2 : TEXCOORD2,     \n"
		"            in float  oVignette  : TEXCOORD3)    : SV_Target                           \n"
		"{                                                                                      \n"
		// 3 samples for fixing chromatic aberrations
		"    float R = Texture.Sample(Linear, oTexCoord0.xy).r;                                 \n"
		"    float G = Texture.Sample(Linear, oTexCoord1.xy).g;                                 \n"
		"    float B = Texture.Sample(Linear, oTexCoord2.xy).b;                                 \n"
		"    return (oVignette*float4(R,G,B,1));                                                \n"
		"}";
	pRender->InitShaders(vertexShader, pixelShader, &Shaders, &VertexIL, DistortionMeshVertexDesc, 6);

    for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
    {
        // Allocate mesh vertices, registering with renderer using the OVR vertex format.
        ovrDistortionMesh meshData;
        ovrHmd_CreateDistortionMesh(HMD, (ovrEyeType) eyeNum, eyeFov[eyeNum],
			                        ovrDistortionCap_Chromatic | ovrDistortionCap_TimeWarp, &meshData);
        MeshVBs[eyeNum] = *pRender->CreateBuffer();
        MeshVBs[eyeNum]->Data(Buffer_Vertex, meshData.pVertexData, sizeof(ovrDistortionVertex)*meshData.VertexCount);
        MeshIBs[eyeNum] = *pRender->CreateBuffer();
        MeshIBs[eyeNum]->Data(Buffer_Index, meshData.pIndexData, sizeof(unsigned short) * meshData.IndexCount);
        ovrHmd_DestroyDistortionMesh( &meshData );

		//Create eye render description for use later
		EyeRenderDesc[eyeNum] = ovrHmd_GetRenderDesc(HMD, (ovrEyeType) eyeNum,  eyeFov[eyeNum]);

		//Do scale and offset
		ovrHmd_GetRenderScaleAndOffset(eyeFov[eyeNum],RenderTargetSize, EyeRenderViewport[eyeNum], UVScaleOffset[eyeNum]);
	}

    #endif

    ovrHmd_SetEnabledCaps(HMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);

	// Start the sensor which informs of the Rift's pose and motion
    ovrHmd_ConfigureTracking(HMD,   ovrTrackingCap_Orientation |
                                    ovrTrackingCap_MagYawCorrection |
                                    ovrTrackingCap_Position, 0);

    // This creates lights and models.
  	pRoomScene = new Scene;
	PopulateRoomScene(pRoomScene, pRender);
	#if !VIDEO_CAPTURE
	PopulateRoomScene(pRoomScene, pRender);
	#endif

    return (0);
}

//-------------------------------------------------------------------------------------
void ProcessAndRender()
{
    static ovrPosef eyeRenderPose[2]; 

	// Start timing
    #if SDK_RENDER
	ovrHmd_BeginFrame(HMD, 0); 
    #else
	ovrHmd_BeginFrameTiming(HMD, 0); 
    // Retrieve data useful for handling the Health and Safety Warning - unused, but here for reference
    ovrHSWDisplayState hswDisplayState;
    ovrHmd_GetHSWDisplayState(HMD, &hswDisplayState);
    #endif

	// Adjust eye position and rotation from controls, maintaining y position from HMD.
	static float    BodyYaw(3.141592f);
	static Vector3f HeadPos(0.0f, 1.6f, -5.0f);
	HeadPos.y = ovrHmd_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, HeadPos.y);
	bool freezeEyeRender = Util_RespondToControls(BodyYaw, HeadPos, eyeRenderPose[1].Orientation);

    pRender->BeginScene();
    
	// Render the two undistorted eye views into their render buffers.
    if (!freezeEyeRender) // freeze to debug for time warp
    {
        pRender->SetRenderTarget ( pRendertargetTexture );
        pRender->SetViewport (Recti(0,0, pRendertargetTexture->GetWidth(),
                                         pRendertargetTexture->GetHeight() ));  
        pRender->Clear();

		#if !VIDEO_CAPTURE
		//VideoCapturingDevice.videoCapture();
		Capture1 >> CvMat1;
		cv::cvtColor(CvMat1, InputMat1, CV_RGB2BGRA);
		pVideoCaptureTexture->UpdateVideoCapture(&InputMat1);

		ShaderFill videoCaptureShaderFill(VideoCapShaders);
		videoCaptureShaderFill.SetTexture(0, pVideoCaptureTexture);
		videoCaptureShaderFill.SetInputLayout(VideoCapVertexIL);//possible to use "pRender->ModelVertexIL"
		#endif
		
		for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
		{
            ovrEyeType eye = HMD->EyeRenderOrder[eyeIndex];
            eyeRenderPose[eye] = ovrHmd_GetEyePose(HMD, eye);

			#if VIDEO_CAPTURE
			//VideoCapturingDevice.videoCapture();
			if (eyeIndex == 0)
			{
				Capture1 >> CvMat1;
				cv::cvtColor(CvMat1, InputMat1, CV_RGB2BGRA);
				pVideoCaptureTexture->UpdateVideoCapture(&InputMat1);
			}
			else
			{
				Capture2 >> CvMat2;
				cv::cvtColor(CvMat2, InputMat2, CV_RGB2BGRA);
				pVideoCaptureTexture->UpdateVideoCapture(&InputMat2);
			}

			ShaderFill videoCaptureShaderFill(VideoCapShaders);
			videoCaptureShaderFill.SetTexture(0, pVideoCaptureTexture);
			videoCaptureShaderFill.SetInputLayout(VideoCapVertexIL);//possible to use "pRender->ModelVertexIL"
			#endif

            // Get view and projection matrices
			Matrix4f rollPitchYaw       = Matrix4f::RotationY(BodyYaw);
			Matrix4f finalRollPitchYaw  = rollPitchYaw * Matrix4f(eyeRenderPose[eye].Orientation);
			Vector3f finalUp            = finalRollPitchYaw.Transform(Vector3f(0,1,0));
			Vector3f finalForward       = finalRollPitchYaw.Transform(Vector3f(0,0,-1));
			Vector3f shiftedEyePos      = HeadPos + rollPitchYaw.Transform(eyeRenderPose[eye].Position);
            Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
			Matrix4f proj = ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, 0.01f, 10000.0f, true);

			pRender->SetViewport(Recti(EyeRenderViewport[eye]));
			pRender->SetProjection(proj);

			#if VIDEO_CAPTURE
			Matrix4f Projection = 
			  { 1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f, //(float)d2Desc.Height / (float)d2Desc.Width
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f };
			VideoCapShaders->SetUniform4x4f("Projection", Projection);  //Nb transposed when set
			pRender->SetDepthMode(false, false, RenderDevice::Compare_Less);
			pRender->Render(&videoCaptureShaderFill, VideoCapVB, VideoCapIB, sizeof(VideoCapVertex));

			pRender->SetDepthMode(true, true);
			pRoomScene->Render(pRender, Matrix4f::Translation(EyeRenderDesc[eye].ViewAdjust) * view);
			#else
			pRender->SetDepthMode(true, true);
			pRoomScene->Render(pRender, Matrix4f::Translation(EyeRenderDesc[eye].ViewAdjust) * view);
			#endif
		}
    }
    pRender->FinishScene();

	#if SDK_RENDER	// Let OVR do distortion rendering, Present and flush/sync
	ovrHmd_EndFrame(HMD, eyeRenderPose, &EyeTexture[0].Texture);
    #else
	// Clear screen
	pRender->SetDefaultRenderTarget();
	pRender->SetFullViewport();
	pRender->Clear(0.0f, 0.0f, 0.0f, 0.0f);

	// Setup shader
	ShaderFill distortionShaderFill(Shaders);
	distortionShaderFill.SetTexture(0, pRendertargetTexture);
	distortionShaderFill.SetInputLayout(VertexIL);

	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
		// Get and set shader constants
		Shaders->SetUniform2f("EyeToSourceUVScale",   UVScaleOffset[eyeNum][0].x, UVScaleOffset[eyeNum][0].y);
		Shaders->SetUniform2f("EyeToSourceUVOffset",  UVScaleOffset[eyeNum][1].x, UVScaleOffset[eyeNum][1].y);
 		ovrMatrix4f timeWarpMatrices[2];
		ovrHmd_GetEyeTimewarpMatrices(HMD, (ovrEyeType)eyeNum, eyeRenderPose[eyeNum], timeWarpMatrices);
		Shaders->SetUniform4x4f("EyeRotationStart", timeWarpMatrices[0]);  //Nb transposed when set
		Shaders->SetUniform4x4f("EyeRotationEnd",   timeWarpMatrices[1]);  //Nb transposed when set
		// Perform distortion
		pRender->Render(&distortionShaderFill, MeshVBs[eyeNum], MeshIBs[eyeNum], sizeof(ovrDistortionVertex));
	}

    unsigned char latencyColor[3];
    ovrBool drawDk2LatencyQuad = ovrHmd_GetLatencyTest2DrawColor(HMD, latencyColor);
    if(drawDk2LatencyQuad)
    {
        const int latencyQuadSize = 20; // only needs to be 1-pixel, but larger helps visual debugging
        pRender->SetViewport(HMD->Resolution.w - latencyQuadSize, 0, latencyQuadSize, latencyQuadSize);
        pRender->Clear(latencyColor[0] / 255.0f, latencyColor[1] / 255.0f, latencyColor[2] / 255.0f, 0.0f);
    }
	pRender->SetDefaultRenderTarget();

	pRender->Present( true ); // Vsync enabled

    // Only flush GPU for ExtendDesktop; not needed in Direct App Renering with Oculus driver.
    if (HMD->HmdCaps & ovrHmdCap_ExtendDesktop)
		pRender->WaitUntilGpuIdle();
  
	ovrHmd_EndFrameTiming(HMD);
    #endif
}

/*
void RenderFramePseudoCode()
{    
ovrFrame hmdFrameState = ovrHmd_BeginFrame(hmd); 

for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
{
    ovrEyeType eye           = HMDDesc.EyeRenderOrder[eyeIndex];
    ovrPosef   eyeRenderPose = ovrHmd_BeginEyeRender(hmd, eye);

    RenderGameView(RenderViewports[eye], eyeRenderPose);

    ovrHmd_EndEyeRender(hmd, eye, &EyeTexture[eye].Texture);
}

// Let OVR do distortion rendering, Present and Flush+Sync.
ovrHmd_EndFrame(hmd);
}
*/ 

//-------------------------------------------------------------------------------------
void Release(void)
{
	if (pRendertargetTexture) pRendertargetTexture->Release();
	#if VIDEO_CAPTURE
	if (pVideoCaptureTexture) pVideoCaptureTexture->Release();
	Capture1.release();
	Capture2.release();
	VideoCapVB.Clear();
	VideoCapIB.Clear();
	VideoCapShaders->UnsetShader(Shader_Vertex);
	VideoCapShaders->UnsetShader(Shader_Pixel);
	VideoCapShaders->Release();
	SerialPort.ClosePort();
	#endif

    #if !SDK_RENDER
	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
		MeshVBs[eyeNum].Clear();
		MeshIBs[eyeNum].Clear();
	}
	if (Shaders)
	{
		Shaders->UnsetShader(Shader_Vertex);
		Shaders->UnsetShader(Shader_Pixel);
        Shaders->Release();
	}
    #endif

    ovrHmd_Destroy(HMD);
    Util_ReleaseWindowAndGraphics(pRender);
    if (pRoomScene) delete pRoomScene;


    // No OVR functions involving memory are allowed after this.
    ovr_Shutdown(); 
}

