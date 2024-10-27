#include "ttftext.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "TTFText System";

        int round(double x)
        {
            return (int)(x + 0.5);
        }

        int nextpoweroftwo(int x)
        {
            double logbase2 = log(x) / log(2);
            return round(pow(2,ceil(logbase2)));
        }
    }

    template <>
    void serialize(Archive& archive, const TTFText& value)
    {
        archive.startSerialization(TTFText::getType());

        serialize(archive, "text", value.text);
        serialize(archive, "size", value.textSize);
        serialize(archive, "colors", value.colors);
        serialize(archive, "fontPath", value.fontPath);

        archive.endSerialization();
    }

    template <>
    TTFText deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Sentence Text");

            TTFText data;

            data.text = deserialize<std::string>(serializedString["text"]);
            data.textSize = deserialize<int>(serializedString["size"]);
            data.colors = deserialize<constant::Vector4D>(serializedString["colors"]);
            data.fontPath = deserialize<std::string>(serializedString["fontPath"]);

            return data;
        }

        return TTFText{};
    }


    TTFTextSystem::TTFTextSystem(MasterRenderer *renderer) : AbstractRenderer(renderer, RenderStage::Render)
    {
        if (FT_Init_FreeType(&ft))
        {
            LOG_ERROR(DOM, "ERROR::FREETYPE: Could not init FreeType Library");
        }
    }

    void TTFTextSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        baseMaterialPreset.shader = masterRenderer->getShader("simpleTexture");

        baseMaterialPreset.nbTextures = 1;

        baseMaterialPreset.nbAttributes = 11;

        baseMaterialPreset.uniformMap.emplace("sWidth", "ScreenWidth");
        baseMaterialPreset.uniformMap.emplace("sHeight", "ScreenHeight");

        baseMaterialPreset.mesh = std::make_shared<SimpleTexturedSquareMesh>(std::vector<size_t>{3, 2, 1, 1, 3, 1});

        auto group = registerGroup<UiComponent, TTFText>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - ttf group !");

            auto ui = entity->get<UiComponent>();
            auto sentence = entity->get<TTFText>();

            ecsRef->attach<TTFTextCall>(entity, createRenderCall(ui, sentence));

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - ttf group !");

            auto entity = ecsRef->getEntity(id);

            ecsRef->detach<TTFTextCall>(entity);

            changed = true;
        });
    }

    void TTFTextSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void TTFTextSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(entityId);
        
        if (not entity or not entity->has<UiComponent>() or not entity->has<TTFTextCall>())
            return; 

        auto ui = entity->get<UiComponent>();
        auto shape = entity->get<TTFText>();

        entity->get<TTFTextCall>()->call = createRenderCall(ui, shape);

        changed = true;
    }

    void TTFTextSystem::execute()
    {
        if (not changed)
            return;

        renderCallList.clear();

        const auto& renderCallView = viewGroup<UiComponent, TTFText, TTFTextCall>();

        changed = false;

        // renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            auto rCall = renderCall->get<TTFTextCall>();

            if (rCall->call.key == 0 and renderCall->get<TTFText>()->text != "")
            {
                rCall->call = createRenderCall(renderCall->get<UiComponent>(), renderCall->get<TTFText>());
                changed = true;
            }
            else
            {
                renderCallList.push_back(rCall->call);
            }
        }

        
    }

    RenderCall TTFTextSystem::createRenderCall(CompRef<UiComponent> ui, CompRef<TTFText> obj)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processUiComponent(ui);

        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        auto textureName = "TTFText_" + obj->text + "_" + std::to_string(obj->textSize);

        if (masterRenderer->hasMaterial(textureName))
        {
            call.setMaterial(masterRenderer->getMaterialID(textureName));
        }
        else if (masterRenderer->hasTexture(textureName))
        {
            Material simpleShapeMaterial = baseMaterialPreset;

            simpleShapeMaterial.textureId[0] = masterRenderer->getTexture(textureName).id;

            call.setMaterial(masterRenderer->registerMaterial(textureName, simpleShapeMaterial));
        }
        else
        {
            auto fontPath = obj->fontPath;
            auto textSize = obj->textSize;
            auto text = obj->text;

            auto f = [fontPath, textSize, text, textureName, this]() {
                // OpenGLTexture fontTexture;

                // TTF_Font * font = TTF_OpenFont(fontPath.c_str(), textSize);

                // if (not font)
                // {
                //     LOG_ERROR(DOM, "Font could not be loaded: " << fontPath << ", error: " << TTF_GetError());

                //     return fontTexture;
                // }

                // SDL_Color color = {255, 255, 255, 255};

                // // SDL_Rect area;
                // // SDL_Surface *sText = SDL_ConvertSurfaceFormat(TTF_RenderUTF8_Blended(font, text.c_str(), color), SDL_PIXELFORMAT_RGBA32, 0);

                // // if (not sText)
                // // {
                // //     LOG_ERROR(DOM, "Font surface could not be generated" << ", error: " << TTF_GetError());
                // // }

                // // area.x = 0; area.y = 0; area.w = sText->w; area.h = sText->h;

                // // SDL_Surface* temp = SDL_CreateRGBSurface(0, sText->w,sText->h,32,0x000000ff,0x0000ff00,0x00ff0000,0x000000ff);

                // // if (not temp)
                // // {
                // //     LOG_ERROR(DOM, "Font surface could not be generated" << ", error: " << TTF_GetError());
                // // }

                // // SDL_BlitSurface(sText, &area, temp, NULL);

                // SDL_Surface * sFont = TTF_RenderText_Blended(font, text.c_str(), color);

                // if (not sFont)
                // {
                //     LOG_ERROR(DOM, "Font surface could not be generated" << ", error: " << TTF_GetError());

                //     TTF_CloseFont(font);

                //     return fontTexture;
                // }

                // // auto colors = sFont->format->BytesPerPixel;

                // // auto texture_format = GL_RGBA;

                // // if (colors == 4)
                // // { // alpha
                // //     if (sFont->format->Rmask == 0x000000ff)
                // //         texture_format = GL_RGBA;
                // //     else
                // //         texture_format = GL_BGRA;
                // // }
                // // else
                // // { // no alpha
                // //     if (sFont->format->Rmask == 0x000000ff)
                // //         texture_format = GL_RGB;
                // //     else
                // //         texture_format = GL_BGR;
                // // }


                // auto w = nextpoweroftwo(sFont->w);
                // auto h = nextpoweroftwo(sFont->h);
                
                // auto intermediary = SDL_CreateRGBSurface(0, w, h, 32, 
                //         0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

                // SDL_BlitSurface(sFont, 0, intermediary, 0);

                // GLuint texture;
                // glGenTextures(1, &texture);
                // glBindTexture(GL_TEXTURE_2D, texture);

                // glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                // // glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
                // // glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                // // glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
                // // glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

                // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                // // // set texture filtering parameters
                // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                // // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sText->w, sText->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp->pixels);
                // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, intermediary->pixels);
                // // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sFont->w, sFont->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sFont->pixels);
                // // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, sFont->pixels);

                // // LOG_INFO(DOM, "Text has size: " << sFont->w << ", " << sFont->h);
                // LOG_INFO(DOM, "Text has size: " << w << ", " << h);
                // // LOG_INFO(DOM, "Text has size: " << sText->w << ", " << sText->h);

                // // sizeMap[textureName] = TTFSize{sFont->w, sFont->h};
                // sizeMap[textureName] = TTFSize{w, h};
                // // sizeMap[textureName] = TTFSize{sText->w, sText->h};

                // fontTexture.id = texture;
                // fontTexture.transparent = false;

                // SDL_FreeSurface(sFont);
                // SDL_FreeSurface(intermediary);
                // // SDL_FreeSurface(sText);
                // // SDL_FreeSurface(temp);
                // TTF_CloseFont(font);

                // return fontTexture;
            };

            // masterRenderer->queueRegisterTexture(textureName, f);

            return RenderCall{};
        }

        if (masterRenderer->getTexture(textureName).transparent)
        {
            call.setOpacity(OpacityType::Additive);
        }
        else
        {
            call.setOpacity(OpacityType::Opaque);
        }

        auto size = sizeMap[textureName];

        obj->textWidth = size.width;
        obj->textHeight = size.height;

        call.data.resize(11);

        call.data[0] = ui->pos.x;
        call.data[1] = ui->pos.y;
        call.data[2] = ui->pos.z;
        call.data[3] = ui->width;
        call.data[4] = ui->height;
        call.data[5] = ui->rotation;
        call.data[6] = obj->colors.w;
        call.data[7] = obj->colors.x;
        call.data[8] = obj->colors.y;
        call.data[9] = obj->colors.z;
        call.data[10] = 1.0f;

        return call;
    }
}