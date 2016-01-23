#ifdef WIN32
// fix bug in visual studio headers
#define NOMINMAX
#endif

#include "Input/Adt/AdtFile.hpp"
#include "Output/Adt.hpp"
#include "Output/Continent.hpp"
#include "parser.hpp"

#include "utility/Include/LinearAlgebra.hpp"
#include "utility/Include/Directory.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <set>
#include <memory>
#include <cassert>
#include <limits>
#include <algorithm>

namespace parser
{
inline bool Adt::IsRendered(unsigned char mask[], int x, int y)
{
    return (mask[y] >> x & 1) != 0;
}

Adt::Adt(Continent *continent, int adtX, int adtY)
    : X(adtX), Y(adtY), m_continent(continent),
      Bounds({
            (32.f - (float)adtY - 1.f)*utility::MathHelper::AdtSize,
            (32.f - (float)adtX - 1.f)*utility::MathHelper::AdtSize,
            std::numeric_limits<float>::max()
        },
        {
            (32.f - (float)adtY)*utility::MathHelper::AdtSize,
            (32.f - (float)adtX)*utility::MathHelper::AdtSize,
            std::numeric_limits<float>::lowest()
        })
{
    std::stringstream ss;

    ss << "World\\Maps\\" << continent->Name << "\\" << continent->Name << "_" << adtX << "_" << adtY << ".adt";

    // parsing
    input::AdtFile adt(ss.str());

    // Process all data into triangles/indices
    // Terrain

    for (int chunkY = 0; chunkY < 16; ++chunkY)
    {
        for (int chunkX = 0; chunkX < 16; ++chunkX)
        {
            auto const mapChunk = adt.m_chunks[chunkY][chunkX].get();

            AdtChunk *const chunk = new AdtChunk();

            int vertCount = 0;

            chunk->m_terrainVertices = std::move(mapChunk->Positions);
            chunk->m_surfaceNormals = std::move(mapChunk->Normals);

            Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, mapChunk->MaxZ);
            Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, mapChunk->MinZ);

            memcpy(chunk->m_holeMap, mapChunk->HoleMap, sizeof(chunk->m_holeMap));

            // build index list to exclude holes (8 * 8 quads, 4 triangles per quad, 3 indices per triangle)
            chunk->m_terrainIndices.reserve(8 * 8 * 4 * 3);

            for (int y = 0; y < 8; ++y)
            {
                for (int x = 0; x < 8; ++x)
                {
                    const int currIndex = y * 17 + x;

                    // if this chunk has holes and this quad is one of them, skip it
                    if (mapChunk->HasHoles && mapChunk->HoleMap[y][x])
                        continue;

                    // Upper triangle
                    chunk->m_terrainIndices.push_back(currIndex);
                    chunk->m_terrainIndices.push_back(currIndex + 9);
                    chunk->m_terrainIndices.push_back(currIndex + 1);

                    // Left triangle
                    chunk->m_terrainIndices.push_back(currIndex);
                    chunk->m_terrainIndices.push_back(currIndex + 17);
                    chunk->m_terrainIndices.push_back(currIndex + 9);

                    // Lower triangle
                    chunk->m_terrainIndices.push_back(currIndex + 9);
                    chunk->m_terrainIndices.push_back(currIndex + 17);
                    chunk->m_terrainIndices.push_back(currIndex + 18);

                    // Right triangle
                    chunk->m_terrainIndices.push_back(currIndex + 1);
                    chunk->m_terrainIndices.push_back(currIndex + 9);
                    chunk->m_terrainIndices.push_back(currIndex + 18);
                }
            }

            m_chunks[chunkY][chunkX].reset(chunk);
        }
    }

    // Water

    if (adt.m_hasMH2O || adt.m_hasMCLQ)
    {
        auto const mh2oBlock = adt.m_liquidChunk.get();

        if (mh2oBlock)
        {
            for (size_t i = 0; i < mh2oBlock->Layers.size(); ++i)
            {
                auto const layer = mh2oBlock->Layers[i].get();
                auto const chunk = m_chunks[layer->Y][layer->X].get();

                adt.m_chunks[layer->Y][layer->X]->HasWater = true;

                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        if (!layer->Render[y][x])
                            continue;

                        int terrainVert = y * 17 + x;
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, layer->Heights[y + 0][x + 0] });

                        terrainVert = y * 17 + (x + 1);
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, layer->Heights[y + 0][x + 1] });

                        terrainVert = (y + 1) * 17 + x;
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, layer->Heights[y + 1][x + 0] });

                        terrainVert = (y + 1) * 17 + (x + 1);
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, layer->Heights[y + 1][x + 1] });

                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, layer->Heights[y + 0][x + 0]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, layer->Heights[y + 0][x + 0]);
                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, layer->Heights[y + 0][x + 1]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, layer->Heights[y + 0][x + 1]);
                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, layer->Heights[y + 1][x + 0]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, layer->Heights[y + 1][x + 0]);
                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, layer->Heights[y + 1][x + 1]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, layer->Heights[y + 1][x + 1]);

                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 4);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);

                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 1);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);
                    }
            }
        }

        for (int chunkY = 0; chunkY < 16; ++chunkY)
            for (int chunkX = 0; chunkX < 16; ++chunkX)
            {
                auto const mclqBlock = adt.m_chunks[chunkY][chunkX]->LiquidChunk.get();

                if (!mclqBlock)
                    continue;

                auto const chunk = m_chunks[chunkY][chunkX].get();

                // i dont think this is possible.  its only here because im curious if it happens anywhere.
                assert(!mh2oBlock);

                // four vertices per square, 8x8 squares (max)
                chunk->m_liquidVertices.reserve(chunk->m_liquidVertices.size() + 4 * 8 * 8);

                // six indices (two triangles) per square
                chunk->m_liquidIndices.reserve(chunk->m_liquidIndices.size() + 6 * 8 * 8);

                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        if (mclqBlock->RenderMap[y][x] == 0xF)
                            continue;

                        // XXX FIXME - this is here because it may be that this bit is what actually controls rendering.  trap with assert to debug
                        assert(mclqBlock->RenderMap[y][x] != 8);

                        int terrainVert = y * 17 + x;
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y][x] });

                        terrainVert = y * 17 + (x + 1);
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y][x + 1] });

                        terrainVert = (y + 1) * 17 + x;
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y + 1][x] });

                        terrainVert = (y + 1) * 17 + (x + 1);
                        chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y + 1][x + 1] });

                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, mclqBlock->Heights[y + 0][x + 0]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, mclqBlock->Heights[y + 0][x + 0]);
                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, mclqBlock->Heights[y + 0][x + 1]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, mclqBlock->Heights[y + 0][x + 1]);
                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, mclqBlock->Heights[y + 1][x + 0]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, mclqBlock->Heights[y + 1][x + 0]);
                        Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, mclqBlock->Heights[y + 1][x + 1]);
                        Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, mclqBlock->Heights[y + 1][x + 1]);

                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 4);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);

                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 1);
                        chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);
                    }
            }
    }

    // WMOs

    if (adt.m_wmoChunk)
    {
        for (size_t i = 0; i < adt.m_wmoChunk->Wmos.size(); ++i)
        {
            auto const &wmoDefinition = adt.m_wmoChunk->Wmos[i];

            const Wmo *wmo;

            // if it has not been loaded, ensure that it is loaded before proceeding 
            if (!continent->IsWmoLoaded(wmoDefinition.UniqueId))
            {
                input::WmoRootFile newWmo(adt.m_wmoNames[adt.m_wmoChunk->Wmos[i].NameId], &adt.m_wmoChunk->Wmos[i]);

                assert(!!newWmo.Vertices.size() && !!newWmo.Indices.size());

                continent->InsertWmo(wmoDefinition.UniqueId, new Wmo(newWmo.Vertices, newWmo.Indices,
                                                                     newWmo.LiquidVertices, newWmo.LiquidIndices,
                                                                     newWmo.DoodadVertices, newWmo.DoodadIndices,
                                                                     newWmo.Bounds));
            }

            wmo = continent->GetWmo(wmoDefinition.UniqueId);

            assert(!!wmo);

            // iterate over all vertices to determine which chunks have the wmo
            for (size_t v = 0; v < wmo->Vertices.size(); ++v)
            {
                auto const &vertex = wmo->Vertices[v];

                // if this vertex doesn't even fall on the adt, there is no way it can land on a chunk
                // if it falls on the edge, always handle it the same way (<= in the min case on both axis)
                if (vertex.X > Bounds.MaxCorner.X || vertex.X <= Bounds.MinCorner.X || vertex.Y > Bounds.MaxCorner.Y || vertex.Y <= Bounds.MinCorner.Y)
                    continue;

                const int chunkX = static_cast<int>((Bounds.MaxCorner.Y - vertex.Y) / utility::MathHelper::AdtChunkSize);
                const int chunkY = static_cast<int>((Bounds.MaxCorner.X - vertex.X) / utility::MathHelper::AdtChunkSize);

                assert(chunkX < 16 && chunkY < 16);

                Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, vertex.Z);
                Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, vertex.Z);

                m_chunks[chunkY][chunkX]->m_wmos.insert(wmoDefinition.UniqueId);
            }
        }
    }

    // Doodads

    if (adt.m_doodadChunk)
    {
        for (unsigned int i = 0; i < adt.m_doodadChunk->Doodads.size(); ++i)
        {
            auto const &doodadDefinition = adt.m_doodadChunk->Doodads[i];

            // if it has already been loaded, skip parsing it again
            if (continent->IsDoodadLoaded(doodadDefinition.UniqueId))
                continue;

            auto const &modelName = adt.m_doodadNames[doodadDefinition.NameId];

            std::unique_ptr<input::Doodad> newDoodad(new input::Doodad(modelName, doodadDefinition));

            if (!newDoodad->Vertices.size() || !newDoodad->Indices.size())
                continue;

            auto const doodad = new Doodad(newDoodad->Vertices, newDoodad->Indices, newDoodad->MinZ, newDoodad->MaxZ);

            continent->InsertDoodad(doodadDefinition.UniqueId, doodad);

            // iterate over all vertices to determine which chunks have the doodad
            for (size_t v = 0; v < doodad->Vertices.size(); ++v)
            {
                auto const &vertex = doodad->Vertices[v];

                // if this vertex doesn't even fall on the adt, there is no way it can land on a chunk
                if (vertex.X > Bounds.MaxCorner.X || vertex.X <= Bounds.MinCorner.X || vertex.Y > Bounds.MaxCorner.Y || vertex.Y <= Bounds.MinCorner.Y)
                    continue;

                const int chunkX = static_cast<int>((Bounds.MaxCorner.Y - vertex.Y) / utility::MathHelper::AdtChunkSize);
                const int chunkY = static_cast<int>((Bounds.MaxCorner.X - vertex.X) / utility::MathHelper::AdtChunkSize);

                assert(chunkX < 16 && chunkY < 16);

                Bounds.MaxCorner.Z = std::max(Bounds.MaxCorner.Z, vertex.Z);
                Bounds.MinCorner.Z = std::min(Bounds.MinCorner.Z, vertex.Z);

                m_chunks[chunkY][chunkX]->m_doodads.insert(doodadDefinition.UniqueId);
            }
        }
    }
}

#ifdef _DEBUG
void Adt::WriteObjFile() const
{
    std::stringstream ss;

    ss << m_continent->Name << "_" << X  << "_" << Y << ".obj";

    std::ofstream out(ss.str());

    unsigned int indexOffset = 1;
    std::set<unsigned int> dumpedWmos;
    std::set<unsigned int> dumpedDoodads;

    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
        {
            auto chunk = m_chunks[y][x].get();

            // terrain vertices
            out << "# terrain vertices (" << chunk->m_terrainVertices.size() << ")" << std::endl;
            for (unsigned int i = 0; i < chunk->m_terrainVertices.size(); ++i)
                out << "v " << -chunk->m_terrainVertices[i].Y
                    << " "  <<  chunk->m_terrainVertices[i].Z
                    << " "  << -chunk->m_terrainVertices[i].X << std::endl;

            // terrain indices
            out << "# terrain indices (" << chunk->m_terrainIndices.size() << ")" << std::endl;
            for (unsigned int i = 0; i < chunk->m_terrainIndices.size(); i += 3)
                out << "f " << indexOffset + chunk->m_terrainIndices[i]
                    << " "  << indexOffset + chunk->m_terrainIndices[i+1]
                    << " "  << indexOffset + chunk->m_terrainIndices[i+2] << std::endl;

            indexOffset += chunk->m_terrainVertices.size();

            // water vertices
            out << "# water vertices (" << chunk->m_liquidVertices.size() << ")" << std::endl;
            for (unsigned int i = 0; i < chunk->m_liquidVertices.size(); ++i)
                out << "v " << -chunk->m_liquidVertices[i].Y
                    << " "  <<  chunk->m_liquidVertices[i].Z
                    << " "  << -chunk->m_liquidVertices[i].X << std::endl;

            // water indices
            out << "# water indices (" << chunk->m_liquidIndices.size() << ")" << std::endl;
            for (unsigned int i = 0; i < chunk->m_liquidIndices.size(); i += 3)
                out << "f " << indexOffset + chunk->m_liquidIndices[i]   << " "
                            << indexOffset + chunk->m_liquidIndices[i+1] << " "
                            << indexOffset + chunk->m_liquidIndices[i+2] << std::endl;

            indexOffset += chunk->m_liquidVertices.size();

            // wmo vertices and indices
            out << "# wmo vertices / indices (including liquids and doodads)" << std::endl;
            for (auto id = chunk->m_wmos.begin(); id != chunk->m_wmos.end(); ++id)
            {
                // if this wmo has already been dumped as part of another chunk, skip it
                if (dumpedWmos.find(*id) != dumpedWmos.end())
                    continue;

                dumpedWmos.insert(*id);

                const Wmo *wmo = m_continent->GetWmo(*id);

                for (unsigned int i = 0; i < wmo->Vertices.size(); ++i)
                    out << "v " << -wmo->Vertices[i].Y
                        << " "  <<  wmo->Vertices[i].Z
                        << " "  << -wmo->Vertices[i].X << std::endl;

                for (unsigned int i = 0; i < wmo->Indices.size(); i+=3)
                    out << "f " << indexOffset + wmo->Indices[i]   << " " 
                                << indexOffset + wmo->Indices[i+1] << " "
                                << indexOffset + wmo->Indices[i+2] << std::endl;

                indexOffset += wmo->Vertices.size();

                for (unsigned int i = 0; i < wmo->LiquidVertices.size(); ++i)
                    out << "v " << -wmo->LiquidVertices[i].Y
                        << " "  <<  wmo->LiquidVertices[i].Z
                        << " "  << -wmo->LiquidVertices[i].X << std::endl;

                for (unsigned int i = 0; i < wmo->LiquidIndices.size(); i+=3)
                    out << "f " << indexOffset + wmo->LiquidIndices[i]   << " " 
                                << indexOffset + wmo->LiquidIndices[i+1] << " "
                                << indexOffset + wmo->LiquidIndices[i+2] << std::endl;

                indexOffset += wmo->LiquidVertices.size();

                for (unsigned int i = 0; i < wmo->DoodadVertices.size(); ++i)
                    out << "v " << -wmo->DoodadVertices[i].Y
                        << " "  <<  wmo->DoodadVertices[i].Z
                        << " "  << -wmo->DoodadVertices[i].X << std::endl;

                for (unsigned int i = 0; i < wmo->DoodadIndices.size(); i+=3)
                    out << "f " << indexOffset + wmo->DoodadIndices[i]   << " " 
                                << indexOffset + wmo->DoodadIndices[i+1] << " "
                                << indexOffset + wmo->DoodadIndices[i+2] << std::endl;

                indexOffset += wmo->DoodadVertices.size();
            }

            // doodad vertices
            out << "# doodad vertices" << std::endl;
            for (auto id = chunk->m_doodads.begin(); id != chunk->m_doodads.end(); ++id)
            {
                // if this wmo has already been dumped as part of another chunk, skip it
                if (dumpedDoodads.find(*id) != dumpedDoodads.end())
                    continue;

                dumpedDoodads.insert(*id);

                const Doodad *doodad = m_continent->GetDoodad(*id);

                for (unsigned int i = 0; i < doodad->Vertices.size(); ++i)
                    out << "v " << -doodad->Vertices[i].Y
                        << " "  <<  doodad->Vertices[i].Z
                        << " "  << -doodad->Vertices[i].X << std::endl;

                for (unsigned int i = 0; i < doodad->Indices.size(); i+=3)
                    out << "f " << indexOffset + doodad->Indices[i]   << " " 
                                << indexOffset + doodad->Indices[i+1] << " "
                                << indexOffset + doodad->Indices[i+2] << std::endl;

                indexOffset += doodad->Vertices.size();
            }
        }

    out.close();
}

size_t Adt::GetWmoCount() const
{
    std::set<unsigned int> wmos;

    for (int chunkY = 0; chunkY < 16; ++chunkY)
        for (int chunkX = 0; chunkX < 16; ++chunkX)
            wmos.insert(m_chunks[chunkY][chunkX]->m_wmos.cbegin(), m_chunks[chunkY][chunkX]->m_wmos.cend());

    return wmos.size();
}
#endif

const AdtChunk *Adt::GetChunk(const int chunkX, const int chunkY) const
{
    return m_chunks[chunkY][chunkX].get();
}
}