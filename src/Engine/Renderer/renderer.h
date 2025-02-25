#pragma once

#include <unordered_map>

#include <mutex>
#include <condition_variable>
#include <atomic>

#include <cstdarg>

#include "ECS/entitysystem.h"

#include "Input/inputcomponent.h"

#include "Loaders/atlasloader.h"

#include "constant.h"
#include "mesh.h"
#include "camera.h"

namespace pg
{    
    // Forwarding
    class OpenGLShaderProgram;
    class OpenGLContext;
    class MasterRenderer;

    // Type def
    typedef constant::RefracTable RefracRef;

    // TODO make a specialized renderer for std::nullptr_t to catch nullptr error;

    class UiComponent;
    struct PositionComponent;

    struct RenderableTexture
    {
        _unique_id entityId;
        CompRef<UiComponent> uiRef;
        Mesh* meshRef;
    };

    enum class RenderStage : uint8_t
    {
        Render      = 0b000,
        PreRender   = 0b001,
        PostProcess = 0b010
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

    struct OpenGLState
    {
        OpenGLState() {}
        OpenGLState(const OpenGLState& rhs) : scissorEnabled(rhs.scissorEnabled), scissorBound(rhs.scissorBound) {}
        OpenGLState(OpenGLState&& rhs) : scissorEnabled(std::move(rhs.scissorEnabled)), scissorBound(std::move(rhs.scissorBound)) {}

        OpenGLState & operator=(const OpenGLState& rhs)
        {
            scissorEnabled = rhs.scissorEnabled;
            scissorBound = rhs.scissorBound;

            return *this;
        }

        OpenGLState & operator=(OpenGLState&& rhs)
        {
            scissorEnabled = std::move(rhs.scissorEnabled);
            scissorBound = std::move(rhs.scissorBound);

            return *this;
        }

        bool operator==(const OpenGLState& rhs) const
        {
            return scissorEnabled == rhs.scissorEnabled and scissorBound == rhs.scissorBound; 
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

        bool scissorEnabled = false;
        constant::Vector4D scissorBound;
    };

    struct RenderCall
    {
        /**
         * This key is used to sort the data for the renderer 
         * This key is a bit field that contains the following data:
         * 
         * 1 bit indicating if the texture is visible or not
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

        RenderCall() {}
        RenderCall(const RenderCall& other) : key(other.key), data(other.data), batchable(other.batchable), state(other.state) {}
        RenderCall(RenderCall&& other) : key(std::move(other.key)), data(std::move(other.data)), batchable(std::move(other.batchable)), state(std::move(other.state)) {}
        ~RenderCall() {};

        RenderCall & operator=(const RenderCall& other)
        {
            key       = other.key;
            data      = other.data;
            batchable = other.batchable;
            state     = other.state;

            return *this;
        }

        RenderCall & operator=(RenderCall&& other)
        {
            key       = std::move(other.key);
            data      = std::move(other.data);
            batchable = std::move(other.batchable);
            state     = std::move(other.state);

            return *this;
        }

        RenderCall(bool visible, const RenderStage& stage, const OpacityType& opacity, int depth, uint64_t materialId)
        {
            setVisibility(visible);
            setRenderStage(stage);
            setOpacity(opacity);
            setDepth(depth);
            setMaterial(materialId);
        }

        void log() const;

        // deprecated
        void processUiComponent(UiComponent *component);

        void processPositionComponent(CompRef<PositionComponent> component);

        void setVisibility(bool visible)
        {
            // Todo reverse visible in the key so that all the visible element are first
            key = (key & ~((uint64_t)0b1 << 63)) | static_cast<uint64_t>(visible) << 63;
        }

        bool getVisibility() const
        {
            return (key >> 63);
        }

        void setRenderStage(const RenderStage& stage)
        {
            key = (key & ~((uint64_t)0b111 << 56)) | static_cast<uint64_t>(stage) << 56;
        }

        RenderStage getRenderStage() const
        {
            uint64_t stageValue = (key >> 56) & 0b111;

            return static_cast<RenderStage>(stageValue);
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
            return getOpacity() != OpacityType::Opaque ? key < other.key : key > other.key;
        }
    };

    class BaseAbstractRenderer
    {
    public:
        BaseAbstractRenderer(MasterRenderer* masterRenderer, const RenderStage& stage);
        virtual ~BaseAbstractRenderer() {}

        RenderStage getRenderStage() const { return renderStage; }

        const std::vector<RenderCall>& getRenderCalls() const { return renderCallList; }

    protected:
        MasterRenderer *masterRenderer;

        std::vector<RenderCall> renderCallList;
    
        RenderStage renderStage;

        bool changed = true;
    };

    class AbstractRenderer : public BaseAbstractRenderer
    {
    public:
        AbstractRenderer(MasterRenderer* masterRenderer, const RenderStage& stage) : BaseAbstractRenderer(masterRenderer, stage) {}
        virtual ~AbstractRenderer() {}

        RenderStage getRenderStage() const { return renderStage; }
    };

    //[TODO] Multiple FBO -> 1 for a whole screen capture and other for batch rendering on a texture 
    // Add Particle system with instancing already done / create an alternative if needed

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

        std::shared_ptr<Mesh> mesh;
    };

    struct SkipRenderPass {};

    // Todo fix crash on renderer when failure to grab a missing texture or shader

    class MasterRenderer : public System<Listener<OnSDLScanCode>, Listener<SkipRenderPass>>
    {
    private:
        struct MaterialHolder
        {
            MaterialHolder(const std::string& name, const Material& material, size_t index) : materialName(name), material(material), index(index) {}
            MaterialHolder(const MaterialHolder& other) : materialName(other.materialName), material(other.material), index(other.index) {}

            MaterialHolder& operator=(const MaterialHolder& other)
            {
                materialName = other.materialName;
                material = other.material;
                index = other.index;

                return *this;
            } 

            std::string materialName;
            Material material;
            size_t index;
        };

        struct TextureRegisteringQueueItem
        {
            std::string name;
            std::function<OpenGLTexture(size_t)> callback;
        };

    public:
        MasterRenderer(const std::string& noneTexturePath = "");
        ~MasterRenderer();

        virtual std::string getSystemName() const override { return "Renderer System"; }

        virtual void onEvent(const OnSDLScanCode& event) override;
        virtual void onEvent(const SkipRenderPass&) override { skipRenderPass = true; }

        virtual void execute() override;

        void processTextureRegister();

        void renderAll();

        void registerShader(const std::string& name, OpenGLShaderProgram *shaderProgram);
        void registerShader(const std::string& name, const std::string& vsPath, const std::string& fsPath);

        OpenGLTexture registerTextureHelper(const std::string& name, const char* texturePath, size_t oldId = 0, bool instantRegister = true);
        void registerTexture(const std::string& name, OpenGLTexture texture) { textureList[name] = texture; }
        void registerTexture(const std::string& name, const char* texturePath);
        void registerAtlasTexture(const std::string& name, const char* texturePath, const char* atlasFilePath);

        void queueRegisterTexture(const std::string& name, const char* texturePath)
        {
            std::string path = texturePath;
            std::function<OpenGLTexture(size_t)> f = [name, path, this](size_t oldId) { return registerTextureHelper(name, path.c_str(), oldId, false); };

            textureRegisteringQueue.enqueue(TextureRegisteringQueueItem{name, f});
        }
        void queueRegisterTexture(const std::string& name, const std::function<OpenGLTexture(size_t)>& callback) { textureRegisteringQueue.enqueue(TextureRegisteringQueueItem{name, callback}); }

        size_t registerMaterial(const Material& material)
        {
            LOG_MILE("Renderer", "Registering a new material");

            std::lock_guard<std::mutex> lock(materialRegisterMutex);
            auto index = nbRegisteredMaterials++;
            materialRegisterQueue.emplace_back("", material, index);

            return index;
        }

        size_t registerMaterial(const std::string& materialName, const Material& material)
        {
            LOG_MILE("Renderer", "Registering a new material: " << materialName);
            std::lock_guard<std::mutex> lock(materialRegisterMutex);
            auto index = nbRegisteredMaterials++;
            materialRegisterQueue.emplace_back(materialName, material, index);

            return index;
        }

        bool hasMaterial(const std::string& materialName) const
        {
            bool result = false; 

            {
                std::lock_guard<std::mutex> lock(materialRegisterMutex);

                auto it = std::find_if(materialRegisterQueue.begin(), materialRegisterQueue.end(), [materialName](const MaterialHolder& holder) { return holder.materialName == materialName; });
                
                result = materialDict.find(materialName) != materialDict.end() or it != materialRegisterQueue.end();
            }

            return result;
        }

        //TODO raise exception on none presence of attribute
        OpenGLShaderProgram* getShader(const std::string& name) const
        {
            try
            {
                return shaderList.at(name);
            }
            catch (const std::exception&)
            {
                LOG_ERROR("Renderer", "Shader named: " << name << " is not available !");

                return nullptr;
            }
        }

        OpenGLTexture getTexture(const std::string& name) const
        { 
            try
            {
                return textureList.at(name);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Renderer", "Texture named " << name << " don't exist !");
                
                auto it = textureList.find("NoneIcon");

                if (it != textureList.end())
                {
                    LOG_INFO("Renderer", "Loading None Icon instead");
                    return it->second;
                }
                else
                    return OpenGLTexture{};
            }
        }

        bool hasTexture(const std::string& name) const
        {
            return textureList.find(name) != textureList.end();
        }

        const LoadedAtlas::AtlasTexture& getAtlasTexture(const std::string& textureName, const std::string& atlasTextureName) const
        {
            return atlasMap.at(textureName).getTexture(atlasTextureName);
        }
        const Material& getMaterial(const std::string& name) const { return materialList.at(materialDict.at(name)); }
        const Material& getMaterial(size_t id) const { return materialList.at(id); }

        size_t getMaterialID(const std::string& name) const
        {
            std::lock_guard<std::mutex> lock(materialRegisterMutex);

            auto it = std::find_if(materialRegisterQueue.begin(), materialRegisterQueue.end(), [name](const MaterialHolder& holder) { return holder.materialName == name; });

            if (it != materialRegisterQueue.end())
                return it->index;
            else
                return materialDict.at(name);
        }

        template <typename... Args>
        void render(const Args&... args) { renderer(this, args...); }

        template <typename Renderable>
        MasterRenderer& operator<<(Renderable* toRender) { renderer(this, toRender); return *this; }

        void setWindowSize(float width, float height)
        { 
            systemParameters["ScreenWidth"] = width;
            systemParameters["ScreenHeight"] = height;
        }

        void setCurrentTime(const unsigned int& time) { systemParameters["CurrentTime"] = static_cast<int>(time); }

        RefracRef& getParameter() { return systemParameters; }

        Camera& getCamera() { return camera; }

        inline void addRenderer(BaseAbstractRenderer* renderer) { renderers.push_back(renderer); }

        inline size_t getNbGeneratedFrames() const { return nbGeneratedFrames; }

        inline size_t getNbRenderedFrames() const { return nbRenderedFrames; }

        inline size_t getNbRenderCall() const { return renderCallList[currentRenderList.load()].size(); }

    private:
        std::atomic<bool> inSwap {false};
        std::atomic<bool> newMaterialRegistered {false};
        // std::atomic<bool> inBetweenRender {true};

        // std::condition_variable execCv;
        // std::condition_variable renderCv;

        mutable std::mutex materialRegisterMutex;
        std::vector<MaterialHolder> materialRegisterQueue;

        moodycamel::ConcurrentQueue<TextureRegisteringQueueItem> textureRegisteringQueue;

        size_t nbRegisteredMaterials = 0;
        
    private:
        void initializeParameters();

        void setState(const OpenGLState& state);

        void processRenderCall(const RenderCall& call);

    private:
        RefracRef systemParameters;
        std::unordered_map<std::string, OpenGLShaderProgram*> shaderList;
        std::unordered_map<std::string, OpenGLTexture> textureList;
        std::vector<Material> materialList;
        std::unordered_map<std::string, size_t> materialDict;

        std::vector<Material> materialListTemp;
        std::unordered_map<std::string, size_t> materialDictTemp;

        size_t nbMaterials = 0;     
   
        /** 
         * Flag to indicate that the current frame should not be recreated (RenderCallList should not be updated)
         * Usefull to avoid any jittering when loading a scene as it takes 2 execute cycle to process all the entities correctly */
        bool skipRenderPass = false;

        Camera camera;

        std::vector<RenderCall> renderCallList[2];

        std::atomic<uint8_t> currentRenderList {0};

        std::unordered_map<std::string, LoadedAtlas> atlasMap;

        size_t nbGeneratedFrames = 0;

        size_t nbRenderedFrames = 0;

        std::vector<BaseAbstractRenderer*> renderers;

        OpenGLState currentState;
    };
}