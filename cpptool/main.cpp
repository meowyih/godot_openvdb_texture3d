#include <iostream>
#include <fstream>
#include <filesystem> // C++17 std::filesystem::exists()

#include <openvdb/openvdb.h>

//
// BMP image functions
//
const int BYTES_PER_PIXEL = 3; // RGB without Alpha
const int FILE_HEADER_SIZE = 14;
const int INFO_HEADER_SIZE = 40;
const int MAX_SLICE_SIZE = 24; // how many slice in a single line, Godot 4.1 Texture3D only accept H:256 V:256 slice at most.

// help function to create BMP file header
unsigned char* createBitmapFileHeader(int height, int stride);

// help function to create BMP image header
unsigned char* createBitmapInfoHeader(int height, int width);


//
// OpeVDB function
//

// read vec3i type metadata in grid
openvdb::Vec3i readVec3iMetadata(const std::string& name, openvdb::GridBase::Ptr baseGrid);

// generate a Texture3D BMP image for Godot usage.
// It writes density into R, flames into G, range is from (0,0,0) to bbox_max
// Texture3D uses slice to store 3D data. The horizontal slice size is MAX_SLICE_SIZE
void convertVDBtoTexture3D(openvdb::FloatGrid::Ptr density, openvdb::FloatGrid::Ptr flames, openvdb::Vec3i bbox_max );

int main()
{
	// check if vdb file exist
	std::string vdb_filename("sample.vdb");

	if (! std::filesystem::exists(vdb_filename)) {
		std::cerr << "Err: VDB file \"" << vdb_filename << "\" does not exist." << std::endl;
		return -1;
	}

	// get 'density' and 'flames' grid from VDB file. Both attributes should be existing in general VDB.
	// You can do the metadata checking if your VDB file has different attribute name or missing either one.
	openvdb::initialize();
	openvdb::io::File file(vdb_filename);
	file.open();
	openvdb::FloatGrid::Ptr density_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(file.readGrid("density"));
	openvdb::FloatGrid::Ptr flames_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(file.readGrid("flames"));
	file.close();

	// get the box range that contains the density and flames voxel. The metadata 'file_bbox_max' and
	// 'file_bbox_min' are in the openVDB generated by JangaFX's Embergen. For any reason your VDB does
	// not have such information, just traversal all the voxel in grid to get the min and max position of
	// the box for VDB simulation
	openvdb::Vec3i bbox_max_density = readVec3iMetadata("file_bbox_max", density_grid);
	openvdb::Vec3i bbox_max_flames = readVec3iMetadata("file_bbox_max", flames_grid);
	openvdb::Vec3i bbox_max(
		std::max(bbox_max_density.x(), bbox_max_flames.x()),
		std::max(bbox_max_density.y(), bbox_max_flames.y()),
		std::max(bbox_max_density.z(), bbox_max_flames.z()));

	// We store density value into pixel's RED value and flames into GREEN value. Since the BMP (24bit format) is 
	// 3 bytes per pixel, each value (float type) will convert into 1 byte data, which is quite imprecise.
	// You can store the whole float value into 3 bytes and create two seperate Texture3D files for density and
	// flames to get more precise result. But it requires more space and some bit operation in Godot shader.
	convertVDBtoTexture3D(density_grid, flames_grid, bbox_max);

	return 0;
}

// read vec3i type metadata in grid
openvdb::Vec3i readVec3iMetadata(const std::string& name, openvdb::GridBase::Ptr baseGrid)
{
	openvdb::Vec3i value(0, 0, 0);

	try
	{
		openvdb::Vec3IMetadata::Ptr metadata = baseGrid->getMetadata<openvdb::Vec3IMetadata>(name);
		value = metadata->value();
	}
	catch (openvdb::TypeError e)
	{
		std::cerr << "Error: readVec3iMetadata() cannot get Vec3I metadata: " << name << " " << e.what() << std::endl;
	}

	return value;
}

// generate a Texture3D BMP image for Godot usage.
// It writes density into R, flames into G, range is from (0,0,0) to bbox_max
// Texture3D uses slice to store 3D data. The horizontal slice size is MAX_SLICE_SIZE
void convertVDBtoTexture3D(openvdb::FloatGrid::Ptr density_grid, openvdb::FloatGrid::Ptr flames_grid, openvdb::Vec3i bbox_max)
{
	// 2D image size for one slice
	int width = bbox_max.x();
	int height = bbox_max.y();

	// allocate seperate buffers for each slice image from z=0 to z= bbox_max.z()
	// NOTE: to save file space, you should also check the bbox_min, reduce the image size to
	// bbox_max.xy - bbox_min.xy and reduce the slice size to bbox_max.z() - bbox_min.z()
	// I will do that *if* I want to make this sample code into real plug-in GDExtension.
	std::map<int, unsigned char*> imgs = std::map<int, unsigned char*>();
	for (int z = 0; z <= bbox_max.z(); z++)
	{
		unsigned char* image = (unsigned char*)new unsigned char[height * width * BYTES_PER_PIXEL];
		std::memset(image, 0.0, height * width * BYTES_PER_PIXEL);
		imgs.insert(std::pair<int, unsigned char*>(z, image));
	}

	// write density in to R byte
	for (openvdb::FloatGrid::ValueOnIter iter = density_grid->beginValueOn(); iter; ++iter)
	{
		unsigned char* buffer;

		int x = iter.getCoord().x();
		int y = iter.getCoord().y();
		int z = iter.getCoord().z();

		buffer = imgs[z];
		buffer[y * width * BYTES_PER_PIXEL + x * BYTES_PER_PIXEL + 2] = (unsigned char)(*iter * 0xFF);
	}

	// write flames in to G byte
	for (openvdb::FloatGrid::ValueOnIter iter = flames_grid->beginValueOn(); iter; ++iter)
	{
		unsigned char* buffer;
		int x = iter.getCoord().x();
		int y = iter.getCoord().y();
		int z = iter.getCoord().z();

		buffer = imgs[z];
		buffer[y * width * BYTES_PER_PIXEL + x * BYTES_PER_PIXEL + 1] = (unsigned char)(*iter * 0xFF);
	}

	// write into bmp file
	int total_block = bbox_max.z() + 1;
	int width_block_count = total_block > MAX_SLICE_SIZE ? MAX_SLICE_SIZE : total_block; // max slice for Texture3D is h:256, v:256
	int widthInBytes = width * width_block_count * BYTES_PER_PIXEL;
	int height_block_count = (total_block - 1) / MAX_SLICE_SIZE + 1;
	int remain_block = total_block;
	int paddingSize = (4 - (widthInBytes) % 4) % 4;
	int stride = (widthInBytes)+paddingSize;
	std::string filename = std::string("t3d_w") + std::to_string(width_block_count) + std::string("_h") + std::to_string(height_block_count) + std::string(".bmp");

	FILE* imageFile = fopen(filename.c_str(), "wb");

	unsigned char* fileHeader = createBitmapFileHeader(height * height_block_count, stride);
	fwrite(fileHeader, 1, FILE_HEADER_SIZE, imageFile);

	unsigned char* infoHeader = createBitmapInfoHeader(height * height_block_count, width * width_block_count);
	fwrite(infoHeader, 1, INFO_HEADER_SIZE, imageFile);

	while (remain_block > 0)
	{
		int start_block_index = total_block - remain_block;
		unsigned char* block_line = new unsigned char[height * stride];
		std::memset(block_line, 0, height * stride);
		for (int idx = start_block_index; idx < start_block_index + MAX_SLICE_SIZE; idx++)
		{
			// int block_line_idx = idx - start_block_index;
			// int block_line_idx = start_block_index + MAX_SLICE_SIZE - idx - 1;
			int block_line_idx = total_block > MAX_SLICE_SIZE ? start_block_index + MAX_SLICE_SIZE - idx - 1 : idx - start_block_index;
			unsigned char* buffer = imgs[idx];

			if (remain_block <= 0)
				break;

			for (int i = 0; i < height; i++)
			{
				std::memcpy(block_line + i * stride + block_line_idx * width * BYTES_PER_PIXEL, buffer + i * width * BYTES_PER_PIXEL, width * BYTES_PER_PIXEL);
			}

			remain_block = remain_block - 1;
		}
		fwrite(block_line, 1, height * stride, imageFile);
		delete[] block_line;
	}

	fclose(imageFile);

	// free buffer
	for (const auto& [key, value] : imgs)
	{
		delete[] value;
	}
	imgs.clear();
}

// help function to create BMP file header
unsigned char* createBitmapFileHeader(int height, int stride)
{
	int fileSize = FILE_HEADER_SIZE + INFO_HEADER_SIZE + (stride * height);

	static unsigned char fileHeader[] = {
		0,0,     /// signature
		0,0,0,0, /// image file size in bytes
		0,0,0,0, /// reserved
		0,0,0,0, /// start of pixel array
	};

	fileHeader[0] = (unsigned char)('B');
	fileHeader[1] = (unsigned char)('M');
	fileHeader[2] = (unsigned char)(fileSize);
	fileHeader[3] = (unsigned char)(fileSize >> 8);
	fileHeader[4] = (unsigned char)(fileSize >> 16);
	fileHeader[5] = (unsigned char)(fileSize >> 24);
	fileHeader[10] = (unsigned char)(FILE_HEADER_SIZE + INFO_HEADER_SIZE);

	return fileHeader;
}

// help function to create BMP image header
unsigned char* createBitmapInfoHeader(int height, int width)
{
	static unsigned char infoHeader[] = {
		0,0,0,0, /// header size
		0,0,0,0, /// image width
		0,0,0,0, /// image height
		0,0,     /// number of color planes
		0,0,     /// bits per pixel
		0,0,0,0, /// compression
		0,0,0,0, /// image size
		0,0,0,0, /// horizontal resolution
		0,0,0,0, /// vertical resolution
		0,0,0,0, /// colors in color table
		0,0,0,0, /// important color count
	};

	infoHeader[0] = (unsigned char)(INFO_HEADER_SIZE);
	infoHeader[4] = (unsigned char)(width);
	infoHeader[5] = (unsigned char)(width >> 8);
	infoHeader[6] = (unsigned char)(width >> 16);
	infoHeader[7] = (unsigned char)(width >> 24);
	infoHeader[8] = (unsigned char)(height);
	infoHeader[9] = (unsigned char)(height >> 8);
	infoHeader[10] = (unsigned char)(height >> 16);
	infoHeader[11] = (unsigned char)(height >> 24);
	infoHeader[12] = (unsigned char)(1);
	infoHeader[14] = (unsigned char)(BYTES_PER_PIXEL * 8);

	return infoHeader;
}