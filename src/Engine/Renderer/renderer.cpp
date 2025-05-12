#define STB_IMAGE_IMPLEMENTATION

#include "renderer.h"

#include <filesystem>
namespace fs = std::filesystem;

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "logger.h"

#include "Helpers/openglobject.h"

#include "Loaders/stb_image.h"

#include "UI/uisystem.h"

#include "2D/position.h"

namespace pg
{
    namespace
    {
        constexpr static const char * const DOM = "Renderer";
    }

    void RenderCall::log() const
    {
        LOG_INFO("RenderCall", "Key: " << key);
        LOG_INFO("RenderCall", "Visible : " << getVisibility() << ", depth: " << getDepth() << ", mat: " << getMaterialId());
        LOG_INFO("RenderCall", "Data size: " << data.size());

        std::string dataString;
        for (const auto value : data)
        {
            dataString += std::to_string(value) + ", ";
        }

        LOG_INFO("RenderCall", "Data values: " << dataString);
    }

    void RenderCall::processUiComponent(UiComponent *component)
    {
        setVisibility(component->isVisible());

        if (not component->isWindowClipped())
        {
            state.scissorEnabled = true;
            float tx = component->clipTopLeft.horizontalAnchor;
            float ty = component->clipTopLeft.verticalAnchor;

            float bx = component->clipBottomRight.horizontalAnchor;
            float by = component->clipBottomRight.verticalAnchor;

            // To get width and height you need to subtract bottom corner to the top corner
            state.scissorBound = constant::Vector4D{tx, ty, bx - tx, by - ty};
        }

        setDepth(component->pos.z);
    }

    void RenderCall::processPositionComponent(CompRef<PositionComponent> component)
    {
        setVisibility(component->isRenderable());

        auto entity = component.getEntity();

        if (entity and entity->has<ClippedTo>())
        {
            auto clippedTo = entity->get<ClippedTo>();

            auto clippedToEntity = component.ecsRef->getEntity(clippedTo->clipperId);

            if (clippedToEntity and clippedToEntity->has<PositionComponent>())
            {
                state.scissorEnabled = true;

                auto position = clippedToEntity->get<PositionComponent>();

                float tx = position->x;
                float ty = position->y;

                float w = position->width;
                float h = position->height;

                // To get width and height you need to subtract bottom corner to the top corner
                state.scissorBound = constant::Vector4D{tx, ty, w, h};
            }
        }

        setDepth(component->z);
    }

    BaseAbstractRenderer::BaseAbstractRenderer(MasterRenderer *masterRenderer, const RenderStage& stage) : masterRenderer(masterRenderer), renderStage(stage)
    {
        LOG_THIS_MEMBER("Base Abstract Renderer");

        masterRenderer->addRenderer(this);
    }

    MasterRenderer::MasterRenderer(const std::string& noneTexturePath)
    {
        LOG_THIS_MEMBER(DOM);

        if (noneTexturePath != "")
        {
            registerTexture("NoneIcon", noneTexturePath.c_str());
        }

        initializeParameters();
    }

    MasterRenderer::~MasterRenderer()
    {
        LOG_THIS_MEMBER(DOM);
    }

    void MasterRenderer::onEvent(const OnSDLScanCode&)
    {
        // Todo
        // switch (event.key)
        // {
        // case SDL_Scancode::SDL_SCANCODE_I:
        //     camera.processCameraMovement(constant::Camera_Movement::FORWARD, 0.1);
        //     break;

        // case SDL_Scancode::SDL_SCANCODE_J:
        //     camera.processCameraMovement(constant::Camera_Movement::LEFT, 0.1);
        //     break;

        // case SDL_Scancode::SDL_SCANCODE_L:
        //     camera.processCameraMovement(constant::Camera_Movement::RIGHT, 0.1);
        //     break;

        // case SDL_Scancode::SDL_SCANCODE_K:
        //     camera.processCameraMovement(constant::Camera_Movement::BACKWARD, 0.1);
        //     break;


        // default:
        //     break;
        // }
    }

    void MasterRenderer::execute()
    {
        // Todo Fix in group and ecs ! ( whereaver we are holding pointer of a comp actually ! )
        // Todo hold a ref to the component list and the component index inside of this list instead of the raw pointer to not get invalidated on resize !

        // inSwap = true;

        // {
        //     std::unique_lock lk(renderMutex);
        //     execCv.notify_all();

        //     renderCv.wait(lk, [this]{ return inBetweenRender.load();});
        // }

        // for (auto renderer : renderers)
        // {
        //     renderer->updateMeshes();
        // }

        if (inSwap)
        {
            return;
        }

        if (newMaterialRegistered)
        {
            return;
        }

        if (materialRegisterQueue.size() > 0)
        {
            materialListTemp.clear();
            materialDictTemp.clear();

            materialListTemp = materialList;
            materialDictTemp = materialDict;

            for (const auto& holder : materialRegisterQueue)
            {
                LOG_MILE(DOM, "Registering material");
                materialListTemp.push_back(holder.material);

                if (holder.materialName != "")
                    materialDictTemp[holder.materialName] = nbMaterials;

                nbMaterials++;
            }

            newMaterialRegistered = true;

            materialRegisterQueue.clear();

            return;
        }

        // If the skip flag is set we unset it and we pass the current render update
        if (skipRenderPass > 0)
        {
            skipRenderPass--;
            return;
        }

        auto tempRenderList = currentRenderList == 0 ? 1 : 0;

        // auto start = std::chrono::steady_clock::now();

        renderCallList[tempRenderList].clear();

        bool isDirty = false;

        for (auto renderer : renderers)
        {
            isDirty |= renderer->isDirty();

            renderer->setDirty(false);
        }

        // Nothing changed since last time we can skip the render pass
        if (not isDirty)
        {
            return;
        }

        std::map<uint64_t, std::vector<RenderCall>> buckets;

        for (auto renderer : renderers)
        {
            const auto& calls = renderer->getRenderCalls();

            for (auto& rc : calls)
            {
                auto& group = buckets[rc.key];
                bool found = false;

                for (auto& call : group)
                {
                    if (call.batchable and rc.batchable and call.state == rc.state)
                    {
                        call.data.insert(call.data.end(), rc.data.begin(), rc.data.end());
                        found = true;
                        break;
                    }
                }

                if (not found)
                    group.emplace_back(std::move(rc));
            }
        }

        std::vector<RenderCall> merged;
        merged.reserve(renderCallList[tempRenderList].size());

        // Todo we can break early when the key visibility is set to false
        // as all the element after the first invisible element will be invisible

        for (auto& pair : buckets)
        {
            for (auto& call : pair.second)
            {
                merged.emplace_back(std::move(call));
            }
        }

        renderCallList[tempRenderList].swap(merged);

        nbGeneratedFrames++;

        if (reRenderAll)
        {
            for (auto r : renderers)
            {
                r->changed = true;
                r->setDirty(true);
            }

            LOG_INFO(DOM, "Re-rendering all the renderers");
            reRenderAll = false;
        }

        inSwap = true;
    }

    void MasterRenderer::registerTexture(const std::string& name, const std::function<OpenGLTexture(size_t)>& callback)
    {
        const auto& it = textureList.find(name);

        size_t oldId = 0;

        if (it == textureList.end())
        {
            LOG_MILE(DOM, "Registering texture: " << name);
        }
        else
        {
            LOG_WARNING(DOM, "Replacing registered texture: " << name);
            oldId = it->second.id;
        }

        auto texture = callback(oldId);

        if (texture.id != 0)
        {
            registerTexture(name, texture);
        }
        else
        {
            LOG_ERROR(DOM, "Trying to register a null texture: " << name);
        }
    }

    void MasterRenderer::processTextureRegister()
    {
        TextureRegisteringQueueItem item;

        bool found = textureRegisteringQueue.try_dequeue(item);

        while (found)
        {
            registerTexture(item.name, item.callback);

            found = textureRegisteringQueue.try_dequeue(item);
        }
    }

    void MasterRenderer::renderAll()
    {
        // Todo Clear screen here

        processTextureRegister();

        for (const auto& call : renderCallList[currentRenderList])
        {
            processRenderCall(call);
        }

        nbRenderedFrames++;

        if (inSwap)
        {
            currentRenderList ^= 1;

            inSwap = false;
        }

        if (newMaterialRegistered)
        {
            std::swap(materialListTemp, materialList);
            std::swap(materialDictTemp, materialDict);

            newMaterialRegistered = false;
        }
    }

    void MasterRenderer::registerShader(const std::string& name, OpenGLShaderProgram *shaderProgram)
    {
        LOG_THIS_MEMBER(DOM);

        shaderList[name] = shaderProgram;
    }

    void MasterRenderer::registerShader(const std::string& name, const std::string& vsPath, const std::string& fsPath)
    {
        LOG_THIS_MEMBER(DOM);

        auto shaderProgram = new OpenGLShaderProgram(vsPath, fsPath);

        registerShader(name, shaderProgram);
    }

    // TODO mirror or not the texture
    // Todo add an argument to specify the type of texture loaded, e.g.: RGBA, RGB, ...
    OpenGLTexture MasterRenderer::registerTextureHelper(const std::string& name, const char* texturePath, size_t oldId, bool instantRegister)
    {
        LOG_THIS_MEMBER(DOM);

        int width, height, nrChannels;
        // Todo change this with our custom file opener (stbi_load_from_memory)

        unsigned char *data = stbi_load(texturePath, &width, &height, &nrChannels, STBI_rgb_alpha);

        if (not data)
        {
            if (stbi_failure_reason())
            {
                LOG_ERROR(DOM, "Failed to load texture: " << texturePath << ", error: " << stbi_failure_reason());
            }
            else
            {
                LOG_ERROR(DOM, "Failed to load texture: Unknown");
            }

            return OpenGLTexture{};
        }

        LOG_INFO(DOM, "Loaded texture " << name << " from " << texturePath << " with width = " << width << " height = " << height << " nbchannels = " << nrChannels);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        unsigned int texture;

        if (oldId)
        {
            texture = oldId;
            glBindTexture(GL_TEXTURE_2D, texture);
        }
        else
        {
            glGenTextures(1, &texture);
        }

        glBindTexture(GL_TEXTURE_2D, texture);
        // set the texture wrapping parameters
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
// #ifdef __EMSCRIPTEN__
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
// #else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // load image, create texture and generate mipmaps
// #endif
        OpenGLTexture tex;

        tex.id = texture;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        tex.transparent = true;

        // Todo this doesn't work properly with fully solid textures
        // if (nrChannels == 4)
        // {
        //     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        //     tex.transparent = true;
        // }
        // else
        // {
        //     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        //     tex.transparent = false;
        // }

        glGenerateMipmap(GL_TEXTURE_2D);

        if (instantRegister)
            registerTexture(name, tex);

        stbi_image_free(data);

        return tex;
    }


    void MasterRenderer::registerTexture(const std::string& name, const char* texturePath)
    {
        registerTextureHelper(name, texturePath);
    }

    void MasterRenderer::registerAtlasTexture(const std::string& name, const char* texturePath, const char* atlasFilePath)
    {
        registerTexture(name, texturePath);

        atlasMap.emplace(name, atlasFilePath);
    }

    void MasterRenderer::setState(const OpenGLState& state)
    {
        if (currentState.scissorEnabled != state.scissorEnabled)
        {
            if (state.scissorEnabled)
            {
                glEnable(GL_SCISSOR_TEST);
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
        }

        if (currentState.scissorBound != state.scissorBound)
        {
            auto& rTable = getParameter();
            const int screenHeight = rTable["ScreenHeight"].get<int>();

            //glScissor defined the box from the bottom left corner (x, y, w, h);
            glScissor(state.scissorBound.x, (screenHeight - state.scissorBound.w) - state.scissorBound.y, state.scissorBound.z, state.scissorBound.w);
        }

        currentState = state;
    }

    void MasterRenderer::printAllDrawCalls()
    {
#ifdef PROFILE

        for (const auto& calls : renderCallList[currentRenderList])
        {
            std::cout << "Call Key:" << calls.key << ", batchable: " << calls.batchable << ", nbElements:" << calls.data.size() << std::endl;
        }

#endif
    }

    void MasterRenderer::processRenderCall(const RenderCall& call)
    {
        if (not call.getVisibility())
            return;

        auto materialId = call.getMaterialId();

        if (materialId >= materialList.size())
        {
            LOG_ERROR(DOM, "Unknown material id: " << materialId);
            return;
        }

        const auto& material = getMaterial(materialId);

        if (material.nbAttributes == 0)
        {
            LOG_ERROR(DOM, "No attributes for this render call");
            return;
        }

        // Todo this should never happens if the material has some attributes
        // if (material.mesh == nullptr)
        // {
        //     LOG_ERROR(DOM, "Mesh not found");
        //     return;
        // }

        // Todo initialize material in another call !
        if (not material.mesh->initialized)
        {
            LOG_MILE(DOM, "Generating mesh");
            material.mesh->generateMesh();
        }

        auto shaderProgram = material.shader;

        if (not shaderProgram)
        {
            LOG_ERROR(DOM, "Shader not found");

            return;
        }

        shaderProgram->bind();

        if (call.state != currentState)
        {
            setState(call.state);
        }

        for (size_t i = 0; i < material.nbTextures; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, material.textureId[i]);
            shaderProgram->setUniformValue("texture" + std::to_string(i), static_cast<int>(i));
        }

        auto& rTable = getParameter();
        const int screenWidth = rTable["ScreenWidth"].get<int>();
        const int screenHeight = rTable["ScreenHeight"].get<int>();

        glm::mat4 projection = glm::mat4(1.0f);
        // glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 scale = glm::mat4(1.0f);

        scale = glm::scale(scale, glm::vec3(2.0f / screenWidth, 2.0f / screenHeight, 1.0f));

        // Todo create a uniform feeder

        for (const auto& uniform : material.uniformMap)
        {
            switch (uniform.second.type)
            {
                case UniformType::INT:
                    shaderProgram->setUniformValue(uniform.first, std::get<int>(uniform.second.value));
                    break;
                case UniformType::FLOAT:
                    shaderProgram->setUniformValue(uniform.first, std::get<float>(uniform.second.value));
                    break;
                case UniformType::VEC2D:
                    shaderProgram->setUniformValue(uniform.first, std::get<glm::vec2>(uniform.second.value));
                    break;
                case UniformType::VEC3D:
                    shaderProgram->setUniformValue(uniform.first, std::get<glm::vec3>(uniform.second.value));
                    break;
                case UniformType::VEC4D:
                    shaderProgram->setUniformValue(uniform.first, std::get<glm::vec4>(uniform.second.value));
                    break;
                case UniformType::MAT4D:
                    shaderProgram->setUniformValue(uniform.first, std::get<glm::mat4>(uniform.second.value));
                    break;
                case UniformType::ID:
                {
                    std::string id = std::get<std::string>(uniform.second.value);

                    const auto& value = rTable[id];

                    switch(value.type)
                    {
                        case ElementType::UnionType::FLOAT:
                            shaderProgram->setUniformValue(uniform.first, value.get<float>());
                            break;
                        case ElementType::UnionType::INT:
                        case ElementType::UnionType::SIZE_T:
                            shaderProgram->setUniformValue(uniform.first, value.get<int>());
                            break;
                        case ElementType::UnionType::BOOL:
                            shaderProgram->setUniformValue(uniform.first, value.get<bool>());
                            break;
                        case ElementType::UnionType::STRING:
                        default:
                        {
                            LOG_ERROR(DOM, "Cannot set uniform for id:" << id << ", Unsupported type :" << value.getTypeString());
                        }
                    }
                }
            }
        }

        shaderProgram->setUniformValue("projection", projection);
        shaderProgram->setUniformValue("model", model);
        shaderProgram->setUniformValue("scale", scale);
        shaderProgram->setUniformValue("view", view);

        material.mesh->bind();

        unsigned int nbElements = call.data.size() / material.nbAttributes;

        if (nbElements == 0)
        {
            LOG_ERROR(DOM, "No elements in the render call should be impossible !");
            return;
        }

        material.mesh->openGLMesh.instanceVBO->allocate(call.data.data(), call.data.size() * sizeof(float));

        if (call.batchable)
        {
            glDrawElementsInstanced(GL_TRIANGLES, material.mesh->modelInfo.nbIndices, GL_UNSIGNED_INT, 0, nbElements);
        }
        else
        {
            glDrawElements(GL_TRIANGLES, material.mesh->modelInfo.nbIndices, GL_UNSIGNED_INT, 0);
        }

        // Todo only release if the shader need to change
        shaderProgram->release();
    }

    void MasterRenderer::initializeParameters()
    {
        LOG_THIS_MEMBER(DOM);

        systemParameters["ScreenWidth"] = 1.0f;
        systemParameters["ScreenHeight"] = 1.0f;
        systemParameters["CurrentTime"] = 1;
    }
}