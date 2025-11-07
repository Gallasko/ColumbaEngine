// Todo

#pragma once

// #include <QOpenGLFunctions>
// #include <QOpenGLTexture>
// #include <QOpenGLVertexArrayObject>
// #include <QOpenGLBuffer>

// #include <vector>
// #include <string>
// #include <map>
// #include <memory>

// #include "../pgconstant.h"

// namespace pg
// {
// 	enum class TileType
// 	{
// 		BLANK,
// 		HOUSE,
// 		SHOP,
// 		ROAD,
// 		TERRAIN,
// 		PATHFINDING,
// 		MISC
// 	};

// 	class TilesLoader
// 	{
// 	public:
// 		class TilesId : protected QOpenGLFunctions
// 		{
// 		friend class TilesLoader;
// 		public:
// 			TilesId();
// 			~TilesId();

// 			inline int getId() const { return id; }
// 			inline std::string getName() const { return name; }
// 			inline QOpenGLVertexArrayObject* getMesh() const { return VAO; }

// 			inline bool operator==(const std::string& rhs) const { return name == rhs; }
// 			inline bool operator==(const TileType& rhs) const { return tileType == rhs; }

// 			inline bool operator!=(const std::string& rhs) const { return !(name == rhs); }
// 			inline bool operator!=(const TileType& rhs) const { return !(tileType == rhs); }

// 			inline constant::SquareInfo getModelInfo() const { return modelInfo; }

// 		protected:
// 			inline void setId(int id) { this->id = id; }
// 			inline void setName(std::string name) { this->name = name; }
// 			void setMesh(int textureId);
// 			void setType(std::string type);

// 		private:
// 			int id = 0;
// 			std::string name = "";
// 			TileType tileType;

// 			constant::SquareInfo modelInfo;

// 			QOpenGLVertexArrayObject *VAO;
// 			QOpenGLBuffer *VBO;
// 			QOpenGLBuffer *EBO;
// 		};

// 		TilesLoader(const std::string& tilesFolder);
// 		~TilesLoader();

// 		const TilesLoader::TilesId* getTile(int id) const;
// 		const TilesLoader::TilesId* getTile(std::string tileName) const;

// 	private:
// 		std::vector<std::shared_ptr<TilesLoader::TilesId>> tilesList;
// 		std::map<std::string, int> tilesDict;
// 		int nbTilesId;
// 	};
// }