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
        RenderCall(std::shared_ptr<Mesh> mesh) : batchable(false), mesh(std::move(mesh)) {}
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

        // deprecated
        void processUiComponent(UiComponent *component);

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
            // Todo find a way (in depth maybe ?) to better order opaque calls as the following does not work correctly
            // This cause a strict weak ordering fault that can cause the engine to crash.
            // return getOpacity() != OpacityType::Opaque ? key < other.key : key > other.key;

            return key < other.key;
        }
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

    protected:
        MasterRenderer *masterRenderer;

        std::vector<RenderCall> renderCallList;

        RenderStage renderStage;

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

        std::shared_ptr<Mesh> mesh = nullptr;
    };

    struct SkipRenderPass { size_t count = 1; };

    struct ReRendererAll { };

    struct SaveCurrentFrameEvent { };

    struct SavedFrameData { std::vector<unsigned char> pixels; const int width = 0; const int height = 0; };

    // Todo fix crash on renderer when failure to grab a missing texture or shader

    class MasterRenderer : public System<Own<BaseCamera2D>, Listener<OnSDLScanCode>, Listener<SkipRenderPass>, Listener<ReRendererAll>, Listener<SaveCurrentFrameEvent>>
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
        virtual void onEvent(const SkipRenderPass& event ) override { skipRenderPass += event.count; }
        virtual void onEvent(const ReRendererAll&) override { reRenderAll = true; }
        virtual void onEvent(const SaveCurrentFrameEvent&) override { saveCurrentFrame = true; }

        virtual void execute() override;

        void processTextureRegister();

        void renderAll();

        void registerShader(const std::string& name, OpenGLShaderProgram *shaderProgram);
        void registerShader(const std::string& name, const std::string& vsPath, const std::string& fsPath);

        OpenGLTexture registerTextureHelper(const std::string& name, const char* texturePath, size_t oldId = 0, bool instantRegister = true);
        void registerTexture(const std::string& name, OpenGLTexture texture) { textureList[name] = texture; }
        void registerTexture(const std::string& name, const char* texturePath);
        void registerAtlasTexture(const std::string& name, const char* texturePath, const char* atlasFilePath, std::unique_ptr<LoadedAtlas> atlas = nullptr);

        void queueRegisterTexture(const std::string& name, const char* texturePath)
        {
            std::string path = texturePath;
            std::function<OpenGLTexture(size_t)> f = [name, path, this](size_t oldId) { return registerTextureHelper(name, path.c_str(), oldId, false); };

            queueRegisterTexture(name, f);
        }

        void queueRegisterTexture(const std::string& name, const std::function<OpenGLTexture(size_t)>& callback)
        {
            if (ecsRef->isRunning())
                textureRegisteringQueue.enqueue(TextureRegisteringQueueItem{name, callback});
            else
                registerTexture(name, callback);

        }

        // Todo change default camera

        size_t queueRegisterCamera(_unique_id camera)
        {
            cameraRegisterQueue.push_back(camera);

            return cameraList.size() + cameraRegisterQueue.size(); // Return the index of the new camera
        }

        void processCameraRegister()
        {
            for (auto id : cameraRegisterQueue)
            {
                auto* camera = ecsRef->getComponent<BaseCamera2D>(id);

                if (not camera)
                {
                    LOG_MILE("Renderer", "Camera " << id << " not found");
                    continue;
                }

                cameraList.push_back(camera);
                ++nbCamera;
            }

            cameraRegisterQueue.clear();
        }

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

        const AtlasTexture& getAtlasTexture(const std::string& textureName, const std::string& atlasTextureName) const
        {
            return atlasMap.at(textureName).getTexture(atlasTextureName);
        }

        const Material& getMaterial(const std::string& name) const
        {
            try
            {
                return materialList.at(materialDict.at(name));
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Renderer", "Material named " << name << " don't exist !");

                static Material dummyMaterial;

                return dummyMaterial;
            }
        }

        const Material& getMaterial(size_t id) const
        {
            try
            {
                return materialList.at(id);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Renderer", "Material id " << id << " don't exist !");

                static Material dummyMaterial;

                return dummyMaterial;
            }
        }

        size_t getMaterialID(const std::string& name) const
        {
            std::lock_guard<std::mutex> lock(materialRegisterMutex);

            auto it = std::find_if(materialRegisterQueue.begin(), materialRegisterQueue.end(), [name](const MaterialHolder& holder) { return holder.materialName == name; });

            if (it != materialRegisterQueue.end())
                return it->index;
            else
            {
                try
                {
                    return materialDict.at(name);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Renderer", "Material named " << name << " don't exist !");

                    return 0;
                }
            }
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

        void printAllDrawCalls();

        inline std::vector<RenderCall> getRenderCalls(int index = -1) const
        {
            if (index < 0 or index >= 2)
                return renderCallList[currentRenderList.load()];

            return renderCallList[index];
        }

    private:
        std::atomic<bool> inSwap {false};
        std::atomic<bool> newMaterialRegistered {false};
        // std::atomic<bool> inBetweenRender {true};

        // std::condition_variable execCv;
        // std::condition_variable renderCv;

        mutable std::mutex materialRegisterMutex;
        std::vector<MaterialHolder> materialRegisterQueue;

        std::vector<_unique_id> cameraRegisterQueue;

        moodycamel::ConcurrentQueue<TextureRegisteringQueueItem> textureRegisteringQueue;

        size_t nbRegisteredMaterials = 0;

        bool reRenderAll = false;

        bool saveCurrentFrame = false;

    private:
        void initializeParameters();

        void setState(const OpenGLState& state);

        void processRenderCall(const RenderCall& call, const RefracRef& rTable, unsigned int screenWidth, unsigned int screenHeight);

        void registerTexture(const std::string& name, const std::function<OpenGLTexture(size_t)>& callback);

        void getFrameData();

    private:
        RefracRef systemParameters;
        std::unordered_map<std::string, OpenGLShaderProgram*> shaderList;
        std::unordered_map<std::string, OpenGLTexture> textureList;
        std::vector<Material> materialList;
        std::unordered_map<std::string, size_t> materialDict;

        std::vector<BaseCamera2D*> cameraList;

        std::vector<Material> materialListTemp;
        std::unordered_map<std::string, size_t> materialDictTemp;

        size_t nbMaterials = 0;

        size_t nbCamera = 0;

        /**
         * Flag to indicate that the current frame should not be recreated (RenderCallList should not be updated)
         * Usefull to avoid any jittering when loading a scene as it takes 2 execute cycle to process all the entities correctly */
        size_t skipRenderPass = 0;

        Camera camera;

        std::vector<RenderCall> renderCallList[2];

        std::atomic<unsigned char> currentRenderList {0};

        std::unordered_map<std::string, LoadedAtlas> atlasMap;

        size_t nbGeneratedFrames = 0;

        size_t nbRenderedFrames = 0;

        std::vector<BaseAbstractRenderer*> renderers;

        OpenGLState currentState;
    };
}