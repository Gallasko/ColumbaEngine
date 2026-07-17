#pragma once

#include <unordered_map>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <variant>

#include <cstdarg>

#include "Memory/concurrentqueue.h"

#include "Renderer/rendercall.h"

#include "Input/sdlevents.h"

#include "Loaders/atlasloader.h"

#include "camera.h"

namespace pg
{
    // Forwarding
    class OpenGLShaderProgram;
    class OpenGLContext;

    // Type def
    typedef constant::RefracTable RefracRef;

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

        bool needRedraw() { return needNewRender; }

        void requestRender() { needNewRender = true; }

        void renderAll();

        void endRender();

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

        void queueRegisterTexture(const std::string& name, const std::function<OpenGLTexture(size_t)>& callback);

        // Todo change default camera

        size_t queueRegisterCamera(_unique_id camera)
        {
            cameraRegisterQueue.push_back(camera);

            return cameraList.size() + cameraRegisterQueue.size(); // Return the index of the new camera
        }

        void processCameraRegister();

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

        // Update an already-registered material's uniform in place. Used by
        // renderers that need to react to runtime state changes (e.g. window
        // resize → new perspective projection). Returns true if the material
        // was found. Also updates any matching entry still sitting in the
        // register queue, so a resize that fires before the first swap still
        // lands on the right material.
        //
        // Safe to call from the main thread: the renderer draws on the main
        // thread too, so there is no write-during-read race against draw().
        bool setMaterialUniform(const std::string& materialName,
                                const std::string& uniformName,
                                const UniformValue& value)
        {
            bool found = false;

            auto it = materialDict.find(materialName);
            if (it != materialDict.end())
            {
                materialList.at(it->second).uniformMap[uniformName] = value;
                found = true;
            }

            // Also patch any pending registration of the same name so the
            // update survives the next swap into materialList.
            {
                std::lock_guard<std::mutex> lock(materialRegisterMutex);

                for (auto& holder : materialRegisterQueue)
                {
                    if (holder.materialName == materialName)
                    {
                        holder.material.uniformMap[uniformName] = value;
                        found = true;
                    }
                }
            }

            if (not found)
                LOG_ERROR("Renderer", "setMaterialUniform: material '"
                    << materialName << "' does not exist");

            return found;
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

        // Todo
        // BaseCamera2D& getCamera() { return camera; }
        Camera& getCamera() { return camera; }

        inline void addRenderer(BaseAbstractRenderer* renderer) { renderers.push_back(renderer); }

        inline size_t getNbMaterials() const { return nbMaterials; }

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
        std::atomic<bool> needNewRender {true};
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

        // Todo
        // BaseCamera2D camera;
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