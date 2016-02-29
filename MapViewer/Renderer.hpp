#pragma once

#include "Camera.hpp"
#include "utility/Include/LinearAlgebra.hpp"

#include <vector>
#include <memory>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <unordered_set>
#include <atlbase.h>

class Renderer
{
    private:
        static const float TerrainColor[4];
        static const float LiquidColor[4];
        static const float WmoColor[4];
        static const float DoodadColor[4];
        static const float BackgroundColor[4];
        static const float MeshColor[4];
        static const float SphereColor[4];
        static const float LineColor[4];
        static const float ArrowColor[4];

        struct ColoredVertex
        {
            utility::Vertex vertex;
            float nx, ny, nz;
            float color[4];
        };

        struct GeometryBuffer
        {
            std::vector<ColoredVertex> VertexBufferCpu;
            std::vector<int> IndexBufferCpu;

            CComPtr<ID3D11Buffer> VertexBufferGpu;
            CComPtr<ID3D11Buffer> IndexBufferGpu;
        };

        const HWND m_window;

        CComPtr<IDXGIFactory2> m_dxgiFactory;
        CComPtr<IDXGISwapChain1> m_swapChain;           // the pointer to the swap chain interface
        CComPtr<ID3D11Device> m_device;                 // the pointer to our Direct3D device interface
        CComPtr<ID3D11DeviceContext> m_deviceContext;   // the pointer to our Direct3D device context
        CComPtr<ID3D11RenderTargetView> m_backBuffer;   // the pointer to our back buffer
        CComPtr<ID3D11InputLayout> m_inputLayout;
        CComPtr<ID3D11VertexShader> m_vertexShader;
        CComPtr<ID3D11PixelShader> m_pixelShader;
        CComPtr<ID3D11Buffer> m_cbPerObjectBuffer;
        CComPtr<ID3D11RasterizerState> m_rasterizerState;
        CComPtr<ID3D11DepthStencilView> m_depthStencilView;
        CComPtr<ID3D11DepthStencilState> m_depthStencilState;
        CComPtr<ID3D11Texture2D> m_depthStencilBuffer;
        CComPtr<ID3D11BlendState> m_liquidBlendState;
        CComPtr<ID3D11BlendState> m_opaqueBlendState;

        std::vector<GeometryBuffer> m_terrainBuffers;
        std::vector<GeometryBuffer> m_liquidBuffers;
        std::vector<GeometryBuffer> m_wmoBuffers;
        std::vector<GeometryBuffer> m_doodadBuffers;
        std::vector<GeometryBuffer> m_meshBuffers;
        std::vector<GeometryBuffer> m_sphereBuffers;
        std::vector<GeometryBuffer> m_lineBuffers;
        std::vector<GeometryBuffer> m_arrowBuffers;

        std::unordered_set<unsigned int> m_wmos;
        std::unordered_set<unsigned int> m_doodads;

        bool m_renderADT;
        bool m_renderLiquid;
        bool m_renderWMO;
        bool m_renderDoodad;
        bool m_renderMesh;
        bool m_renderPathfind;

        void InsertBuffer(std::vector<GeometryBuffer> &buffer, const float *color, const std::vector<utility::Vertex> &vertices, const std::vector<int> &indices, bool genNormals = true);

    public:
        Renderer(HWND window);

        void ClearBuffers();
        void ClearSprites();

        void AddTerrain(const std::vector<utility::Vertex> &vertices, const std::vector<int> &indices);
        void AddLiquid(const std::vector<utility::Vertex> &vertices, const std::vector<int> &indices);
        void AddWmo(unsigned int id, const std::vector<utility::Vertex> &vertices, const std::vector<int> &indices);
        void AddDoodad(unsigned int id, const std::vector<utility::Vertex> &vertices, const std::vector<int> &indices);
        void AddMesh(const std::vector<utility::Vertex> &vertices, const std::vector<int> &indices);
        void AddSphere(const utility::Vertex& position, float size, int recursionLevel = 2);
        void AddLine(const utility::Vertex& start, const utility::Vertex& end);
        void AddArrows(const utility::Vertex& start, const utility::Vertex& end, float step);
        void AddPath(const std::vector<utility::Vertex> &path);

        bool HasWmo(unsigned int id) const;
        bool HasDoodad(unsigned int id) const;

        void Render();

        void SetWireframe(bool enabled);
        void SetRenderADT(bool enabled) { m_renderADT = enabled; }
        void SetRenderLiquid(bool enabled) { m_renderLiquid = enabled; }
        void SetRenderWMO(bool enabled) { m_renderWMO = enabled; }
        void SetRenderDoodad(bool enabled) { m_renderDoodad = enabled; }
        void SetRenderMesh(bool enabled) { m_renderMesh = enabled; }

        bool HitTest(int x, int y, utility::Vertex &out) const;

        Camera m_camera;
};