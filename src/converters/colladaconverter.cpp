/*
 * Plant Mock-Up Converter
 *
 * Copyright (c) 2013, EDF. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */

// Only compile if OpenCOLLADA is available
#ifdef OPENCOLLADASW_FOUND

#include "colladaconverter.h"

#include <COLLADASWPrimitves.h>
#include <COLLADASWInputList.h>
#include <COLLADASWSource.h>
#include <COLLADASWLibraryGeometries.h>

#include <iostream>
#include <set>

#include "../api/rvmcolorhelper.h"
#include "../api/rvmmeshhelper.h"
#include "../api/rvmmeshhelper2.h"

using namespace std;
using namespace COLLADASW;

namespace colladaKeys {
	enum keys {
		node,
		name,
		asset,
		keywords,
		translate,
		matrix,
		instance_geometry,
		url,
		bind_material,
		technique_common,
		instance_material,
		symbol,
		target,
		bind_vertex_input,
		semantic,
		input_semantic,
		input_set,
		contributor,
		author,
		authoring_tool,
		comments,
		up_axis,
		library_effects,
		effect,
		id,
		profile_COMMON,
		technique,
		sid,
		lambert,
		diffuse,
		color,
		library_materials,
		material,
		instance_effect,
		library_visual_scenes,
		visual_scene,
		scene,
		instance_visual_scene,
		geometry,
		library_geometries,
		mesh,
		source,
		float_array,
		count,
		accessor,
		stride,
		param,
		type,
		triangles,
		input,
		offset,
		p,
		ph,
		h,
		polygons,
	};
};

static String colladaKey[] = {
	"node",
	"name",
	"asset",
	"keywords",
	"translate",
	"matrix",
	"instance_geometry",
	"url",
	"bind_material",
	"technique_common",
	"instance_material",
	"symbol",
	"target",
	"bind_vertex_input",
	"semantic",
	"input_semantic",
	"input_set",
	"contributor",
	"author",
	"authoring_tool",
	"comments",
	"up_axis",
	"library_effects",
	"effect",
	"id",
	"profile_COMMON",
	"technique",
	"sid",
	"lambert",
	"diffuse",
	"color",
	"library_materials",
	"material",
	"instance_effect",
	"library_visual_scenes",
	"visual_scene",
	"scene",
	"instance_visual_scene",
	"geometry",
	"library_geometries",
	"mesh",
	"source",
	"float_array",
	"count",
	"accessor",
	"stride",
	"param",
	"type",
	"triangles",
	"input",
	"offset",
	"p",
	"ph",
	"h",
	"polygons",
};

// From http://paulbourke.net/geometry/pointlineplane/lineline.c
bool intersects(const Vector3F& p1, const Vector3F& p2, const Vector3F& p3, const Vector3F& p4) {
    if (p1.equals(p2) || p1.equals(p3) || p1.equals(p4) || p2.equals(p3) || p2.equals(p4) || p3.equals(p4)) {
        return false;
    }
    Vector3F p13 = p1 - p3;
    Vector3F p43 = p4 - p3;
    if (abs(p43[0]) < .001 && abs(p43[1]) < .001 && abs(p43[2]) < .001)
        return false;
    Vector3F p21 = p2 - p1;
    if (abs(p21[0]) < .001 && abs(p21[1]) < .001 && abs(p21[2]) < .001)
        return false;

    float d1343 = p13 * p43;
    float d4321 = p43 * p21;
    float d1321 = p13 * p21;
    float d4343 = p43 * p43;
    float d2121 = p21 * p21;

    double denom = d2121 * d4343 - d4321 * d4321;
    if (abs(denom) < 0.001) {
        return false;
    }
    double numer = d1343 * d4321 - d1321 * d4343;

    float x1 = float(numer / denom);
    float x2 = (d1343 + d4321 * x1) / d4343;
    if (x1 > 1 || x1 < 0 || x2 > 1 || x2 < 0)
        return false;

    Vector3F pa = p1 + p21 * x1;
    Vector3F pb = p3 + p43 * x2;

    Vector3F papb = pb - pa;
    float n2 = papb * papb;
    if (n2 > 0.001)
        return false;

    return true;
}

class CCGroup {
    public:
        CCGroup(const std::string& name, const std::vector<float>& translation, const int& materialId) : m_name(name), m_translation(translation), m_material(materialId) {}

        void addGeometry(const string& name, const vector<float>& matrix) { m_geometries.push_back(pair<string, vector<float> >(name, matrix)); }
        CCGroup& addGroup(const CCGroup& group) { m_groups.push_back(group); return m_groups.back(); }
        void addMetaData(const string& key, const string& value) { m_metaData.push_back(pair<string, string>(key, value)); }

        void writeGroup(COLLADASW::StreamWriter* writer) {
            writer->openElement(colladaKey[colladaKeys::node]);
            writer->appendAttribute(colladaKey[colladaKeys::name], m_name);
            if (!m_metaData.empty()) {
                writer->openElement(colladaKey[colladaKeys::asset]);
                string data;
                for (unsigned int i = 0; i < m_metaData.size(); i++) {
                    data += m_metaData[i].first + ": " +  m_metaData[i].second + "\n";
                }
                writer->appendTextElement(colladaKey[colladaKeys::keywords], data);
                writer->closeElement(); // asset
            }
            writer->openElement(colladaKey[colladaKeys::translate]);
            writer->appendValues(m_translation[0], m_translation[1], m_translation[2]);
            writer->closeElement(); // translate
            for (unsigned int i = 0; i < m_geometries.size(); i++) {
                writer->openElement(colladaKey[colladaKeys::node]);
                writer->openElement(colladaKey[colladaKeys::matrix]);
                vector<float> m(16, 0.f);
                for (unsigned int j = 0; j < 4; j++) {
                    for (unsigned int k = 0; k < 3; k++) {
                        m[j+k*4] = m_geometries[i].second[j*3+k];
                    }
                }
                m[15] = 1.f;
                writer->appendValues(m);
                writer->closeElement(); // matrix
                writer->openElement(colladaKey[colladaKeys::instance_geometry]);
                writer->appendAttribute(colladaKey[colladaKeys::url], "#" + m_geometries[i].first);
                writer->openElement(colladaKey[colladaKeys::bind_material]);
                writer->openElement(colladaKey[colladaKeys::technique_common]);
                writer->openElement(colladaKey[colladaKeys::instance_material]);
                writer->appendAttribute(colladaKey[colladaKeys::symbol], "geometryMaterial");
                writer->appendAttribute(colladaKey[colladaKeys::target], "#M" + to_string((long long) m_material));
                writer->openElement(colladaKey[colladaKeys::bind_vertex_input]);
                writer->appendAttribute(colladaKey[colladaKeys::semantic], "UVSET0");
                writer->appendAttribute(colladaKey[colladaKeys::input_semantic], "TEXCOORD");
                writer->appendAttribute(colladaKey[colladaKeys::input_set], 0);
                writer->closeElement(); // bind_vertex_input
                writer->closeElement(); // instance_material
                writer->closeElement(); // technique_common
                writer->closeElement(); // bind_material
                writer->closeElement(); // instance_geometry
                writer->closeElement(); // node
            }
            for (unsigned int i = 0; i < m_groups.size(); i++) {
                m_groups[i].writeGroup(writer);
            }
            writer->closeElement(); // node
        }

    private:
        string m_name;
        vector<float> m_translation;
        int m_material;
        vector<pair<string, vector<float> > > m_geometries;
        vector<CCGroup> m_groups;
        vector<pair<string, string> > m_metaData;
};

class CCModel {
    public:
        CCModel(const string& projectName, const string& name) : m_projectName(projectName), m_name(name), m_baseGroup("", vector<float>(3,0), 0), m_geometryId(0) {
            m_groupStack.push_back(&m_baseGroup);
        }

        CCGroup& group() { return m_baseGroup; }
        vector<CCGroup*>& groupStack() { return m_groupStack; }
        int& geometryId() { return m_geometryId; }
        set<int>& materialIds() { return m_materialIds; }

    private:
        string m_projectName;
        string m_name;
        CCGroup m_baseGroup;
        vector<CCGroup*> m_groupStack;
        int m_geometryId;
        set<int> m_materialIds;

};

COLLADAConverter::COLLADAConverter(const string& filename)
    : RVMReader(),
      m_model(0) {
    m_writer = new StreamWriter(NativeString(filename));
}

COLLADAConverter::~COLLADAConverter() {
    delete m_writer;
}

void COLLADAConverter::startDocument() {
    m_writer->startDocument();
}

void COLLADAConverter::endDocument() {
    m_writer->endDocument();
}

void COLLADAConverter::startHeader(const string& banner, const string& fileNote, const string& date, const string& user, const string& encoding) {
    m_writer->openElement(colladaKey[colladaKeys::asset]);
    m_writer->openElement(colladaKey[colladaKeys::contributor]);
    m_writer->appendTextElement(colladaKey[colladaKeys::author], user);
    m_writer->appendTextElement(colladaKey[colladaKeys::authoring_tool], banner);
    m_writer->appendTextElement(colladaKey[colladaKeys::comments], fileNote);
    m_writer->appendTextElement(colladaKey[colladaKeys::up_axis], "Z_UP");
    m_writer->closeElement();
}

void COLLADAConverter::endHeader() {
    m_writer->closeElement(); // asset
}

void COLLADAConverter::startModel(const string& projectName, const string& name) {
    m_model = new CCModel(projectName, name);
    m_writer->openElement(colladaKey[colladaKeys::library_geometries]);

    vector<float> origin;
    origin.push_back(0.); origin.push_back(0.); origin.push_back(0.);
    m_translations.push_back(origin);
}

void COLLADAConverter::endModel() {
    m_writer->closeElement(); // library_geometries

    // Effects for materials
    m_writer->openElement(colladaKey[colladaKeys::library_effects]);
    for (set<int>::iterator it = m_model->materialIds().begin(); it != m_model->materialIds().end(); it++) {
        m_writer->openElement(colladaKey[colladaKeys::effect]);
        m_writer->appendAttribute(colladaKey[colladaKeys::id], "E" + to_string((long long)*it));
        m_writer->openElement(colladaKey[colladaKeys::profile_COMMON]);
        m_writer->openElement(colladaKey[colladaKeys::technique]);
        m_writer->appendAttribute(colladaKey[colladaKeys::sid], "COMMON");
        m_writer->openElement(colladaKey[colladaKeys::lambert]);
        m_writer->openElement(colladaKey[colladaKeys::diffuse]);
        m_writer->openElement(colladaKey[colladaKeys::color]);
        vector<float> color = RVMColorHelper::color(*it);
        m_writer->appendValues(color[0], color[1], color[2], 0);
        m_writer->closeElement(); // color
        m_writer->closeElement(); // diffuse
        m_writer->closeElement(); // lambert
        m_writer->closeElement(); // technique
        m_writer->closeElement(); // profile
        m_writer->closeElement(); // effect
    }
    m_writer->closeElement();

    // Materials
    m_writer->openElement(colladaKey[colladaKeys::library_materials]);
    for (set<int>::iterator it = m_model->materialIds().begin(); it != m_model->materialIds().end(); it++) {
        m_writer->openElement(colladaKey[colladaKeys::material]);
        m_writer->appendAttribute(colladaKey[colladaKeys::id], "M" + to_string((long long)*it));
        m_writer->openElement(colladaKey[colladaKeys::instance_effect]);
        m_writer->appendAttribute(colladaKey[colladaKeys::url], "#E" + to_string((long long)*it));
        m_writer->closeElement(); // instance_effect
        m_writer->closeElement(); // material
    }
    m_writer->closeElement(); // library_materials

    // Visual scene
    m_writer->openElement(colladaKey[colladaKeys::library_visual_scenes]);
    m_writer->openElement(colladaKey[colladaKeys::visual_scene]);
    m_writer->appendAttribute(colladaKey[colladaKeys::id], "SCENE");
    m_model->group().writeGroup(m_writer);
    m_writer->closeElement(); // library_visual_scenes
    m_writer->closeElement(); // visual_scene

    // Scene
    m_writer->openElement(colladaKey[colladaKeys::scene]);
    m_writer->openElement(colladaKey[colladaKeys::instance_visual_scene]);
    m_writer->appendAttribute(colladaKey[colladaKeys::url], "#SCENE");
    m_writer->closeElement(); // instance_visual_scene
    m_writer->closeElement(); // scene
}

void COLLADAConverter::startGroup(const std::string& name, const std::vector<float>& translation, const int& materialId) {
    vector<float> t = translation;
    t[0] -= m_translations.back()[0];
    t[1] -= m_translations.back()[1];
    t[2] -= m_translations.back()[2];
    CCGroup group(name, t, materialId);
    CCGroup* lastGroup = m_model->groupStack().back();
    m_model->groupStack().push_back(&lastGroup->addGroup(group));
    m_model->materialIds().insert(materialId);
    m_translations.push_back(translation);
}

void COLLADAConverter::endGroup() {
    m_model->groupStack().pop_back();
    m_translations.pop_back();
}

void COLLADAConverter::startMetaData() {
}

void COLLADAConverter::endMetaData() {
}

void COLLADAConverter::startMetaDataPair(const string &name, const string &value) {
    m_model->groupStack().back()->addMetaData(name, value);
}

void COLLADAConverter::endMetaDataPair() {
}

void COLLADAConverter::startPyramid(const vector<float>& matrix,
                          const float& xbottom,
                          const float& ybottom,
                          const float& xtop,
                          const float& ytop,
                          const float& height,
                          const float& xoffset,
                          const float& yoffset) {
	m_writer->appendTextBlock("<!-- RVMPyramid -->");
    writeMesh(matrix, RVMMeshHelper2::makePyramid(xbottom, ybottom, xtop, ytop, height, xoffset, yoffset, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endPyramid() {
    // in start.
}

void COLLADAConverter::startBox(const vector<float>& matrix,
                      const float& xlength,
                      const float& ylength,
                      const float& zlength) {
	m_writer->appendTextBlock("<!-- RVMBox -->");
	writeMesh(matrix, RVMMeshHelper2::makeBox(xlength, ylength, zlength, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endBox() {
    // in start.
}

void COLLADAConverter::startRectangularTorus(const vector<float>& matrix,
                                   const float& rinside,
                                   const float& routside,
                                   const float& height,
                                   const float& angle) {
    m_writer->appendTextBlock("<!-- RVMRectangularTorus -->");
	writeMesh(matrix, RVMMeshHelper2::makeRectangularTorus(rinside, routside, height, angle, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endRectangularTorus() {
    // in start.
}

void COLLADAConverter::startCircularTorus(const vector<float>& matrix,
                                const float& rinside,
                                const float& routside,
                                const float& angle) {
    m_writer->appendTextBlock("<!-- RVMCircularTorus -->");
    writeMesh(matrix, RVMMeshHelper2::makeCircularTorus(rinside, routside, angle, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endCircularTorus() {
    // in start.
}

void COLLADAConverter::startEllipticalDish(const vector<float>& matrix,
                                 const float& diameter,
                                 const float& radius) {
	m_writer->appendTextBlock("<!-- RVMSphericalDish -->");
    writeMesh(matrix, RVMMeshHelper2::makeEllipticalDish(diameter, radius, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endEllipticalDish() {
    // in start.
}

void COLLADAConverter::startSphericalDish(const vector<float>& matrix,
                                const float& diameter,
                                const float& height) {
    m_writer->appendTextBlock("<!-- RVMSphericalDish -->");
    writeMesh(matrix, RVMMeshHelper2::makeSphericalDish(diameter, height, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endSphericalDish() {
    // in start.
}

void COLLADAConverter::startSnout(const vector<float>& matrix,
                        const float& dtop,
                        const float& dbottom,
                        const float& height,
                        const float& xoffset,
                        const float& yoffset,
                        const float& unknown1,
                        const float& unknown2,
                        const float& unknown3,
                        const float& unknown4) {
	m_writer->appendTextBlock("<!-- RVMSnout -->");
    writeMesh(matrix, RVMMeshHelper2::makeSnout(dtop, dbottom, height, xoffset, yoffset, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endSnout() {
    // in start.
}

void COLLADAConverter::startCylinder(const vector<float>& matrix,
                           const float& radius,
                           const float& height) {
	m_writer->appendTextBlock("<!-- RVMCylinder -->");
    writeMesh(matrix, RVMMeshHelper2::makeCylinder(radius, height, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endCylinder() {
    // in start.
}

void COLLADAConverter::startSphere(const vector<float>& matrix,
                         const float& diameter) {
    m_writer->appendTextBlock("<!-- RVMSphere -->");
    writeMesh(matrix, RVMMeshHelper2::makeSphere(diameter, m_maxSideSize, m_minSides));
}

void COLLADAConverter::endSphere() {
    // in start.
}

void COLLADAConverter::startLine(const vector<float>& matrix, const float& thickness, const float& length) {

    string gid = "G" + to_string((long long)m_model->geometryId()++);

    m_writer->appendTextBlock("<!-- RVMLine -->");
    m_writer->openElement(colladaKey[colladaKeys::geometry]);
    m_writer->appendAttribute(colladaKey[colladaKeys::id], gid);
    m_writer->openElement(colladaKey[colladaKeys::mesh]);

    FloatSourceF positionSource(m_writer);
    positionSource.setId(gid+"C");
    positionSource.setArrayId(gid+"CA");
    positionSource.setAccessorCount(2);
    positionSource.setAccessorStride(3);
    positionSource.getParameterNameList().push_back("X");
    positionSource.getParameterNameList().push_back("Y");
    positionSource.getParameterNameList().push_back("Z");
    positionSource.prepareToAppendValues();


    vector<float> a;
    a.push_back(-length/2.0f); a.push_back(0); a.push_back(0); a.push_back(length/2.0f); a.push_back(0); a.push_back(0);
    m_writer->appendValues(a);
    positionSource.finish();

    Lines lines(m_writer);
    lines.setCount((unsigned int) 1);
    lines.setMaterial("geometryMaterial");
    lines.getInputList().push_back(Input(InputSemantic::POSITION, URI("#" + positionSource.getId()), 0));

    lines.prepareToAppendValues();
    lines.appendValues(0);
    lines.appendValues(1);
    lines.finish();
    //
    m_writer->closeElement(); // mesh
    m_writer->closeElement(); // geometry

    m_model->groupStack().back()->addGeometry(gid, matrix);
}

void COLLADAConverter::endLine() {
    // in start.
}

void COLLADAConverter::startFacetGroup(const vector<float>& matrix,
                             const vector<vector<vector<Vertex> > >& vertexes) {

    string gid = "G" + to_string((long long)m_model->geometryId()++);
    m_writer->appendTextBlock("<!-- RMVFacetGroup -->");
    
    TesselationUserData userData;
    RVMMeshHelper2::tesselateFacetGroup(vertexes, &userData);

    m_writer->openElement(colladaKey[colladaKeys::geometry]);
    m_writer->appendAttribute(colladaKey[colladaKeys::id], gid);
    m_writer->openElement(colladaKey[colladaKeys::mesh]);
    
    // Write coordinates source
    FloatSourceF positionSource(m_writer);
    positionSource.setId(gid+"C");
    positionSource.setArrayId(gid+"CA");
    positionSource.setAccessorCount(userData.positions.size() / 3);
    positionSource.setAccessorStride(3);
    positionSource.getParameterNameList().push_back("X");
    positionSource.getParameterNameList().push_back("Y");
    positionSource.getParameterNameList().push_back("Z");
    positionSource.prepareToAppendValues();
    positionSource.appendValues(userData.positions);
    positionSource.finish();

   
    // Write normals source
    FloatSourceF normalSource(m_writer);
    normalSource.setId(gid+"N");
    normalSource.setArrayId(gid+"NA");
    normalSource.setAccessorCount(userData.normals.size() / 3);
    normalSource.setAccessorStride(3);
    normalSource.getParameterNameList().push_back("X");
    normalSource.getParameterNameList().push_back("Y");
    normalSource.getParameterNameList().push_back("Z");
    normalSource.prepareToAppendValues();
    normalSource.appendValues(userData.normals);
    normalSource.finish();

    // Write polygons
    Triangles p(m_writer);
    p.setCount(userData.indices.size() / 3);
	p.setMaterial("geometryMaterial");
	p.getInputList().push_back(Input(InputSemantic::POSITION, URI("#" + gid + "C"), 0));
    p.getInputList().push_back(Input(InputSemantic::NORMAL, URI("#" + gid + "N"), 0));
	p.prepareToAppendValues();
    p.appendValues(userData.indices);
    p.finish();
    
    m_writer->closeElement(); // mesh
    m_writer->closeElement(); // geometry

    vector<float> m = matrix;
    m[9] -= m_translations.back()[0];
    m[10] -= m_translations.back()[1];
    m[11] -= m_translations.back()[2];
    m_model->groupStack().back()->addGeometry(gid, m);
}

void COLLADAConverter::endFacetGroup() {
    // in start.
}

void COLLADAConverter::writeMesh(const std::vector<float>& matrix, const Mesh &mesh) {
	string gid = "G" + to_string((long long)m_model->geometryId()++);
    bool hasNormals = mesh.normals.size() > 0;
	bool hasNormalIndex = mesh.normalIndex.size() > 0;

	m_writer->openElement(colladaKey[colladaKeys::geometry]);
    m_writer->appendAttribute(colladaKey[colladaKeys::id], gid);
    m_writer->openElement(colladaKey[colladaKeys::mesh]);

	FloatSourceF positionSource(m_writer);
	positionSource.setId(gid+"C");
	positionSource.setArrayId(gid+"CA");
	positionSource.setAccessorCount(mesh.positions.size());
	positionSource.setAccessorStride(3);
	positionSource.getParameterNameList().push_back("X");
	positionSource.getParameterNameList().push_back("Y");
	positionSource.getParameterNameList().push_back("Z");
	positionSource.prepareToAppendValues();
	m_writer->appendValues(&mesh.positions.front()[0], mesh.positions.size() * 3);
    positionSource.finish();

	if(hasNormals) {
		FloatSourceF normalSource(m_writer);
		normalSource.setId(gid+"N");
		normalSource.setArrayId(gid+"NA");
		normalSource.setAccessorCount(mesh.normals.size());
		normalSource.setAccessorStride(3);
		normalSource.getParameterNameList().push_back("X");
		normalSource.getParameterNameList().push_back("Y");
		normalSource.getParameterNameList().push_back("Z");
		normalSource.prepareToAppendValues();
		m_writer->appendValues(&mesh.normals.front()[0], mesh.normals.size() * 3);
		normalSource.finish();
	}

	Triangles t(m_writer);
	t.setCount(mesh.positionIndex.size() / 3);
	t.setMaterial("geometryMaterial");
	t.getInputList().push_back(Input(InputSemantic::POSITION, URI("#" + gid + "C"), 0));
	if(hasNormals) {
		if(hasNormalIndex) {
			t.getInputList().push_back(Input(InputSemantic::NORMAL, URI("#" + gid + "N"), 1));
		} else {
			t.getInputList().push_back(Input(InputSemantic::NORMAL, URI("#" + gid + "N"), 0));
		}
	}
	t.prepareToAppendValues();
	if(hasNormalIndex) {
		for(size_t i = 0; i < mesh.positionIndex.size(); i++) {
			t.appendValues(mesh.positionIndex.at(i));
			t.appendValues(mesh.normalIndex.at(i));
		}
	} else {
		// Just use the position index
		t.appendValues(mesh.positionIndex);
	}
	t.finish();

	m_writer->closeElement(); // mesh
    m_writer->closeElement(); // geometry

    vector<float> m = matrix;
    m[9] -= m_translations.back()[0];
    m[10] -= m_translations.back()[1];
    m[11] -= m_translations.back()[2];
    m_model->groupStack().back()->addGeometry(gid, m);
}

#endif // OPENCOLLADASW_FOUND
