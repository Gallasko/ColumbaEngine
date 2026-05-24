#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>

#include "glm/glm.hpp"

#include "ECS/system.h"

#include "pgconstant.h"
#include "mesh.h"

#include "logger.h"

namespace pg
{
    // Forwarding
    class OpenGLShaderProgram;
    class MasterRenderer;

    struct PositionComponent;

    enum class RenderStage : uint8_t
    {
        Render      = 0b0000,
        PreRender   = 0b0001,
        PostProcess = 0b0010
    };

    enum class OpacityType : uint8_t
    {
        Opaque      = 0b00,
        Normal      = 0b01,
        Additive    = 0b10,
        Subtractive = 0b11
    };

    struct OpenGLTexture
    {
        OpenGLTexture() {}
        OpenGLTexture(const OpenGLTexture& rhs) : id(rhs.id), transparent(rhs.transparent) {}

        OpenGLTexture & operator=(const OpenGLTexture& rhs)
        {
            id = rhs.id;
            transparent = rhs.transparent;

            return *this;
        }

        unsigned int id = 0;
        bool transparent = false;
    };

    // GL constant mirrors so OpenGLState can live in a header without GL includes.
    enum class DepthFunc : uint32_t
    {
        Never    = 0x0200, // GL_NEVER
        Less     = 0x0201, // GL_LESS
        Equal    = 0x0202, // GL_EQUAL
        LEqual   = 0x0203, // GL_LEQUAL
        Greater  = 0x0204, // GL_GREATER
        NotEqual = 0x0205, // GL_NOTEQUAL
        GEqual   = 0x0206, // GL_GEQUAL
        Always   = 0x0207  // GL_ALWAYS
    };

    enum class BlendFactor : uint32_t
    {
        Zero             = 0,      // GL_ZERO
        One              = 1,      // GL_ONE
        SrcAlpha         = 0x0302, // GL_SRC_ALPHA
        OneMinusSrcAlpha = 0x0303, // GL_ONE_MINUS_SRC_ALPHA
        DstAlpha         = 0x0304, // GL_DST_ALPHA
        OneMinusDstAlpha = 0x0305  // GL_ONE_MINUS_DST_ALPHA
    };

    struct OpenGLState
    {
        bool operator==(const OpenGLState& rhs) const
        {
            return scissorEnabled == rhs.scissorEnabled
               and scissorBound   == rhs.scissorBound
               and depthTestEnabled == rhs.depthTestEnabled
               and depthFunc       == rhs.depthFunc
               and blendEnabled    == rhs.blendEnabled
               and blendSrc        == rhs.blendSrc
               and blendDst        == rhs.blendDst;
        }

        bool operator!=(const OpenGLState& rhs) const
        {
            return not (*this == rhs);
        }

        void setScissor(float x, float y, float w, float h)
        {
            scissorEnabled = true;

            scissorBound.x = x;
            scissorBound.y = y;
            scissorBound.z = w;
            scissorBound.w = h;
        }

        // Scissor
        bool scissorEnabled = false;
        constant::Vector4D scissorBound;

        // Depth
        bool depthTestEnabled = false;
        DepthFunc depthFunc   = DepthFunc::Less;

        // Blend (defaults match engine startup: standard alpha blending)
        bool blendEnabled     = true;
        BlendFactor blendSrc  = BlendFactor::SrcAlpha;
        BlendFactor blendDst  = BlendFactor::OneMinusSrcAlpha;
    };

    struct RenderCall
    {
        /**
         * This key is used to sort the data for the renderer
         * This key is a bit field that contains the following data:
         *
         * 1 bit indicating if the texture is visible or not (visible == 0, invisible == 1)
         * 4 bits for the targeted rendering pass
         * 3 bits for the target viewport
         * 2 bits for the translucency type (Opaque, normal, additive or substractive)
         * 24 bits for depth
         * 30 bits for material ID (VAO, shader, texture ID, uniforms)
         *
         * key[63]            => Visibility
         * key[62] -- key[59] => Rendering pass
         * key[58] -- key[56] => Viewport
         * key[55] -- key[54] => Translucency type
         * key[53] -- key[30] => Depth
         * key[29] -- key[0]  => Material ID
         */
        uint64_t key = 0;

        /** All the data stored of this render call */
        std::vector<float> data;

        /** Flag indicating if this call can be batch with other similar call (key with the same value) */
        bool batchable = true;

        /** Keep track of all the opengl state needed for this render call */
        OpenGLState state;

        /** Mesh to use for rendering (If given in ctor, else it will be fetched from the material) */
        std::shared_ptr<Mesh> mesh;

        /** (Internal) Number of elements to render */
        size_t nbElements = 0;

        RenderCall() {}
        RenderCall(std::shared_ptr<Mesh> mesh) : batchable(false), mesh(mesh) {}
        RenderCall(const RenderCall& other) : key(other.key), data(other.data), batchable(other.batchable), state(other.state), mesh(other.mesh), nbElements(other.nbElements) {}
        RenderCall(RenderCall&& other) : key(std::move(other.key)), data(std::move(other.data)), batchable(std::move(other.batchable)), state(std::move(other.state)), mesh(std::move(other.mesh)), nbElements(std::move(other.nbElements)) {}
        ~RenderCall() {};

        RenderCall & operator=(const RenderCall& other)
        {
            key        = other.key;
            data       = other.data;
            batchable  = other.batchable;
            state      = other.state;
            mesh       = other.mesh;
            nbElements = other.nbElements;

            return *this;
        }

        RenderCall & operator=(RenderCall&& other)
        {
            key        = std::move(other.key);
            data       = std::move(other.data);
            batchable  = std::move(other.batchable);
            state      = std::move(other.state);
            mesh       = std::move(other.mesh);
            nbElements = std::move(other.nbElements);

            return *this;
        }

        RenderCall(bool visible, const RenderStage& stage, const OpacityType& opacity, int depth, uint64_t materialId, uint8_t viewport = 0)
        {
            setVisibility(visible);
            setRenderStage(stage);
            setViewport(viewport);
            setOpacity(opacity);
            setDepth(depth);
            setMaterial(materialId);
        }

        void log() const;

        void processPositionComponent(CompRef<PositionComponent> component);

        void setVisibility(bool visible)
        {
            key = (key & ~((uint64_t)0b1 << 63)) | static_cast<uint64_t>(not visible) << 63;
        }

        bool getVisibility() const
        {
            return !(key >> 63);
        }

        void setRenderStage(const RenderStage& stage)
        {
            key = (key & ~((uint64_t)0b1111 << 59)) | static_cast<uint64_t>(stage) << 59;
        }

        RenderStage getRenderStage() const
        {
            uint64_t stageValue = (key >> 59) & 0b1111;

            return static_cast<RenderStage>(stageValue);
        }

        void setViewport(uint8_t viewport)
        {
            if (viewport > 0b111) // Ensure the viewport value fits in 3 bits
            {
                LOG_ERROR("RenderCall", "Viewport value is too large [" << viewport << "], should be less than or equal to 7.");
                return;
            }

            key = (key & ~((uint64_t)0b111 << 56)) | (static_cast<uint64_t>(viewport) << 56);
        }

        uint8_t getViewport() const
        {
            return static_cast<uint8_t>((key >> 56) & 0b111);
        }

        void setOpacity(const OpacityType& opacity)
        {
            key = (key & ~((uint64_t)0b11 << 54)) | static_cast<uint64_t>(opacity) << 54;
        }

        OpacityType getOpacity() const
        {
            uint64_t opacityValue = (key >> 54) & 0b11;

            return static_cast<OpacityType>(opacityValue);
        }

        void setDepth(int depth)
        {
            uint64_t normalizedDepth = (uint64_t)(0b000000000000111111111111) + (int64_t)(depth);
            if (normalizedDepth > 0b111111111111111111111111)
            {
                LOG_ERROR("Render call", "Depth is too far from origin[" << depth << "], should be lesser than: " << 0b111111111111 << " in either positiv or negativ");
            }

            key = (key & ~((uint64_t)0b111111111111111111111111 << 30)) | normalizedDepth << 30;
        }

        int getDepth() const
        {
            return static_cast<int>((key >> 30) & 0b111111111111111111111111) - 0b000000000000111111111111;
        }

        void setMaterial(uint64_t materialId)
        {
            if (materialId > 0b111111111111111111111111111111)
            {
                LOG_ERROR("Render call", "Material id is too big[" << materialId <<"], should be lesser than: " << 0b111111111111111111111111111111);
            }

            key = (key & ~((uint64_t)0b111111111111111111111111111111 << 0)) | static_cast<uint64_t>(materialId) << 0;
        }

        uint64_t getMaterialId() const
        {
            return key & 0b111111111111111111111111111111;
        }

        bool operator<(const RenderCall& other) const
        {
            return key < other.key;
        }
    };

    enum class UniformType
    {
        INT,
        FLOAT,
        ID,
        VEC2D,
        VEC3D,
        VEC4D,
        MAT4D
    };

    struct UniformValue
    {
        UniformValue() : value(static_cast<int>(0)), type(UniformType::INT) {}

        template<typename Type>
        explicit UniformValue(const Type& v) { setValue(v); }

        void setValue(int v)
        {
            type = UniformType::INT;
            value = v;
        }

        void setValue(float v)
        {
            type = UniformType::FLOAT;
            value = v;
        }

        void setValue(const std::string& v)
        {
            type = UniformType::ID;
            value = v;
        }

        void setValue(const glm::vec2& v)
        {
            type = UniformType::VEC2D;
            value = v;
        }

        void setValue(const glm::vec3& v)
        {
            type = UniformType::VEC3D;
            value = v;
        }

        void setValue(const glm::vec4& v)
        {
            type = UniformType::VEC4D;
            value = v;
        }

        void setValue(const glm::mat4& v)
        {
            type = UniformType::MAT4D;
            value = v;
        }

        std::variant<int, float, std::string, glm::vec2, glm::vec3, glm::vec4, glm::mat4> value;

        UniformType type;
    };

    struct Material
    {
        Material() {}
        Material(const Material& rhs) : shader(rhs.shader), nbTextures(rhs.nbTextures), nbAttributes(rhs.nbAttributes), uniformMap(rhs.uniformMap), mesh(rhs.mesh)
        {
            for (size_t i = 0; i < nbTextures; ++i)
            {
                textureId[i] = rhs.textureId[i];
            }
        }

        Material & operator=(const Material& rhs)
        {
            shader = rhs.shader;
            nbTextures = rhs.nbTextures;
            nbAttributes = rhs.nbAttributes;
            uniformMap = rhs.uniformMap;
            mesh = rhs.mesh;

            for (size_t i = 0; i < nbTextures; ++i)
            {
                textureId[i] = rhs.textureId[i];
            }

            return *this;
        }

        void setSimpleMesh(const std::vector<size_t>& attributes)
        {
            mesh = std::make_shared<SimpleTexturedSquareMesh>(attributes);

            for (const auto& value : attributes)
            {
                nbAttributes += value;
            }
        }

        OpenGLShaderProgram* shader;

        // Todo limit the number of texture to 16 max
        size_t nbTextures = 1;
        unsigned int textureId[16] = {0};

        /** Number of attributes per elements in render call */
        size_t nbAttributes = 0;

        std::unordered_map<std::string, UniformValue> uniformMap;

        std::shared_ptr<Mesh> mesh = nullptr;
    };

    class BaseAbstractRenderer
    {
        friend class MasterRenderer;
    public:
        BaseAbstractRenderer(MasterRenderer* masterRenderer, const RenderStage& stage);
        virtual ~BaseAbstractRenderer() {}

        RenderStage getRenderStage() const { return renderStage; }

        const std::vector<RenderCall>& getRenderCalls() const { return renderCallList; }

        void finishChanges() { changed = false; dirty = true; }

        inline void setDirty(bool dirty) { this->dirty = dirty; }

        bool isDirty() const { return dirty; }

        bool hasChanged() const { return changed; }

        MasterRenderer* getMasterRenderer() const { return masterRenderer; }

    protected:
        MasterRenderer *masterRenderer;

        std::vector<RenderCall> renderCallList;

        RenderStage renderStage;

        // GL state applied to every render call produced by this renderer.
        // Subclasses configure this in setupRenderer().
        OpenGLState defaultState;

        bool changed = true;
        bool dirty = true;
    };

    class AbstractRenderer : public BaseAbstractRenderer
    {
    public:
        AbstractRenderer(MasterRenderer* masterRenderer, const RenderStage& stage) : BaseAbstractRenderer(masterRenderer, stage) {}
        virtual ~AbstractRenderer() {}

        RenderStage getRenderStage() const { return renderStage; }
    };
}
