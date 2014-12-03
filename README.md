Oculus-FPV-with-VR
==================

Oculus First Private View with Virtual Reality based on Oculus Tiny, using VC++, Xbee, Arduino Tech Set.
The real image is captured through the stereo camera, Oculus HMD will show these image.

##Index
1. Visual C++ & DirectX HLSL
2. Arduino Servo and Xbee Controller
3. Fixtures used in this sample

**To find out more, please check out the [website] 

## Visual C++ & DirectX HLSL
Since DirectX require 3D programing technique even if it's simple 2D image rendering, this sample use DirectX and HLSL code to display Video image.

Key part is bellow.

''''c
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
		
''''

The way of vertex definition for Video Capture image is bellow. The CounterClockwise parameter should be FALSE, when CreateRasterizerState method is executed(default is FALSE).
''''c
	const VideoCapVertex CapVertex[] = {
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ {  1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ { -1.0f,-1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ { -1.0f,-1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ {  1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
			{ {  1.0f,-1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } } };		
''''


## Arduino Servo and Xbee Controller 
Check Oculus_ex_lcd.ino, this program is assumed to use Arduino UNO, Arduino Shield, and Xbee.
The "ArduinoXbeeLibrary" source code is required when compile "Oculus_ex_lcd.ino".
More Information for this source code see [xbee].


##Fixtures used in this sample

The fixtures used this sample is bellow.

| No | Item                         | Qty | Description                  |
| ---|:----------------------------:| ---:|-----------------------------:|
| 1  | Arduino UNO                  | 1   |                              |
| 2  | Arduino Wireless SD Shield   | 1   |                              |
| 3  | LCD Character Display        | 1   | SD1602HULB(-XA-G-G)          |
| 4  | XBeeZB Module                | 2   | To R/S Headtraking Info      |
| 5  | USB Explore for XBee         | 1   |                              |
| 6  | Camera                       | 2   | 2SW-410AM00C000              |
| 7  | Tranmitter                   | 2   | TS 351 Wireless Transmitter  |
| 8  | Reciever                     | 2   | RC305 Wireless Reciever      |
| 9  | USB Mpeg Capture             | 2   | Buffalo PC-SDVD/U2G          |


* **Camera** The Camera of FOV used in this sample is 90 degree. But FOV used in distortion is around 110. It's better to syncronize the FOV to get better presence.

* **Resolution** Although the resolution for the Camera is 780*480, the program set 360*240 resolution because of USB bandwith issue.
''''c
	Capture1.set(CV_CAP_PROP_FRAME_WIDTH,  360); 
	Capture1.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
''''


[website]: https://achychy911.wordpress.com
[xbee]:http://www.geocities.jp/bokunimowakaru/








