//-
// ==========================================================================
// Copyright 2015 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+

////////////////////////////////////////////////////////////////////////
// DESCRIPTION:
//
// Adds the new file format Bvh to the file manipulation dialogs.
// 
// As soon as this plug-in is loaded, the new file format will be available in
// the "Open", "Import, and "Export" dialogs.
//
// The icon that is displayed in the file selection boxes is contained in the
// file "bvhTranslator.rgb", which is also located in the example
// plug-in directory. Maya will find this icon as long as the path to the
// directory that contains it is included in the FILE_ICON_PATH environment variable.
//
// A "Bvh" file is an ASCII file with a first line of "<BVH>".
// The remainder of the file contains MEL commands that create one of
// these primitives: nurbsSphere, nurbsCone, and nurbsCylinder, as well as move
// commands to position them.
//
// When writing the file, only primitives of these three types will be created
// along with their positions in 3D space. The reader routine will actually handle
// more MEL commands than these, but only this limited set of types will be written.
//
// Additionally, this example demonstrates how to utilize file options.
// When saving a file, if you click on the option box beside the
// File > Export All menu item, a dialog appears that contains two radio boxes asking
// whether to "Write Positions". The default is true, and if false is selected, then the
// move commands for primitives will not be written to the output file. This dialog is
// implemented by the MEL script "bvhTranslatorOpts.mel", which is also located in
// the plug-in directory.
//
// A sample input file is supplied in the example plug-in directory as "bvhTranslator.bvh".
//  
// This example plugin demonstrates how to implement a Maya File Translator.
// The BVH files can be referenced by Maya files.
//  
// Note that this is a simple example.  Hence, there are limitations.
// For example, every geometry saved will have its values reset to default,
// except their translation if the option "Show Position" has been turned on. To find what 
// geometries we can export, we search them by name. Hence, if a polygon cube contains in its 
// name the string "nurbsSphere", it will be written out as a nurbs sphere.
//
////////////////////////////////////////////////////////////////////////


#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MFnPlugin.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <maya/MStringArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MItSelectionList.h>
#include <maya/MSelectionList.h>
#include <maya/MFileIO.h>
#include <maya/MFnTransform.h>
#include <maya/MNamespace.h>
#include <maya/MFnIkJoint.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <ios>
#include <vector>
#include <deque>

class Node {
private:
public:
    std::string name;
    float offset[3];
    std::vector<std::string> channels;
    MObject jointObj;

    Node* parent = nullptr;
    std::vector<Node*> children = std::vector<Node*>();
    std::vector<std::vector<float>> channelValues = std::vector< std::vector<float>>();
    Node();
    Node(std::string name, float offset[3], std::vector<std::string> channels);
    ~Node();

    void mayaCreate();
};

Node::Node(){}

Node::Node(std::string name, float offset[3], std::vector<std::string> channels) {
    this->name = name;
    this->offset[0] = offset[0];
    this->offset[1] = offset[1];
    this->offset[2] = offset[2];
    this->channels = channels;
}

Node::~Node() {}

void Node::mayaCreate(){
    MFnIkJoint jointFn;
    if (parent) {
        jointObj = jointFn.create(parent->jointObj);
    }
    else {
        jointObj = jointFn.create();
    }
    MString name(name.c_str());
    jointFn.setName(name);
    MVector translation(offset);
    jointFn.setTranslation(translation, MSpace::kObject);

    for (Node* child : children) {
        child->mayaCreate();
    }
}



//This is the backbone for creating a MPxFileTranslator
class BvhTranslator : public MPxFileTranslator {
public:

    //Constructor
    BvhTranslator () {};
    //Destructor
               ~BvhTranslator () override {};

    //This tells maya that the translator can read files.
    //Basically, you can import or load with your translator.
    bool haveReadMethod() const override { return true; }

    //This tells maya that the translator can write files.
    //Basically, you can export or save with your translator.
    bool haveWriteMethod() const override { return false; }

    //If this method returns true, and the bvh file is referenced in a scene, the write method will be
    //called when a write operation is performed on the parent file.  This use is for users who wish
    //to implement a custom file referencing system.
    //For this example, we will return false as we will use Maya's file referencing system.
    bool haveReferenceMethod() const override { return false; }

    //If this method returns true, it means we support namespaces.
    bool haveNamespaceSupport()    const override { return true; }

    //This method is used by Maya to create instances of the translator.
    static void* creator();
    
    //This returns the default extension ".bvh" in this case.
    MString defaultExtension () const override;

    //If this method returns true it means that the translator can handle opening files 
    //as well as importing them.
    //If the method returns false then only imports are handled. The difference between 
    //an open and an import is that the scene is cleared(e.g. 'file -new') prior to an 
    //open, which may affect the behaviour of the translator.
    bool canBeOpened() const override { return true; }

    //Maya will call this method to determine if our translator
    //is capable of handling this file.
    MFileKind identifyFile (    const MFileObject& fileName,
                                                const char* buffer,
                                                short size) const override;

    //This function is called by maya when import or open is called.
    MStatus reader ( const MFileObject& file,
                                        const MString& optionsString,
                            MPxFileTranslator::FileAccessMode mode) override;

    bool readNode(Node& node, std::istringstream& tokens);

    bool readAnimNode(Node& node, std::istringstream& tokens, float currentTime);

private:
};

//Creates one instance of the BvhTranslator
void* BvhTranslator::creator()
{
    return new BvhTranslator();
}

// An BVH file is an ascii whose first line contains the string <BVH>.
// The read does not support comments, and assumes that the each
// subsequent line of the file contains a valid MEL command that can
// be executed via the "executeCommand" method of the MGlobal class.
//
MStatus BvhTranslator::reader ( const MFileObject& file,
                                const MString& options,
                                MPxFileTranslator::FileAccessMode mode)
{    
    const MString fname = file.expandedFullName();

    MStatus rval(MS::kSuccess);

    std::ifstream inputfile(fname.asChar(), std::ios::in);
    if (!inputfile) {
        // open failed
        std::cerr << fname << ": could not be opened for reading\n";
        return MS::kFailure;
    }

    std::stringstream buffer;
    buffer << inputfile.rdbuf();
    std::string content = buffer.str();

    std::istringstream tokens(content);

    std::string currentToken;

    tokens >> currentToken;
    if (currentToken.compare("HIERARCHY") != 0) {
        std::cerr << "Error in file content with tokens\n";
        return MS::kFailure;
    }

    tokens >> currentToken;

    std::vector<Node*> rootVect;
    std::deque<Node*> nodeQueue;

    while (currentToken.compare("ROOT") == 0) {
        Node* root = new Node();
        bool state = readNode(*root, tokens);
        if (!state) {
            return MS::kFailure;
        }
        rootVect.push_back(root);
        nodeQueue.push_back(root);
        while (!nodeQueue.empty()) {
            tokens >> currentToken;
            if (currentToken.compare("JOINT") == 0) {
                Node* node = new Node();
                bool state = readNode(*node, tokens);
                if (!state) {
                    return MS::kFailure;
                }
                nodeQueue.back()->children.push_back(node);
                node->parent = nodeQueue.back();
                nodeQueue.push_back(node);
            }
            else if (currentToken.compare("End") == 0) {
                Node* node = new Node();
                tokens >> node->name;
                tokens >> currentToken;
                if (currentToken.compare("{") != 0) {
                    std::cerr << "Error in file content with tokens\n";
                    return MS::kFailure;
                }
                tokens >> currentToken;
                if (currentToken.compare("OFFSET") != 0) {
                    std::cerr << "Error in file content with tokens\n";
                    return MS::kFailure;
                }
                for (int i = 0; i < 3; i++) {
                    tokens >> currentToken;
                    node->offset[i] = std::stof(currentToken);
                }
                nodeQueue.back()->children.push_back(node);
                node->parent = nodeQueue.back();
                tokens >> currentToken;
                if (currentToken.compare("}") != 0) {
                    std::cerr << "Error in file content with tokens\n";
                    return MS::kFailure;
                }
            }
            else if (currentToken.compare("}") == 0) {
                nodeQueue.pop_back();
            }
            else {
                std::cerr << "Error in file content with tokens\n";
                return MS::kFailure;
            }
        }
        tokens >> currentToken;
    }

    if (currentToken.compare("MOTION") != 0) {
        std::cerr << "Error in file content with tokens\n";
        return MS::kFailure;
    }

    tokens >> currentToken;

    if (currentToken.compare("Frames:") != 0) {
        std::cerr << "Error in file content with tokens\n";
        return MS::kFailure;
    }

    tokens >> currentToken;

    int nbFrames = std::stoi(currentToken);

    tokens >> currentToken;

    if (currentToken.compare("Frame") != 0) {
        std::cerr << "Error in file content with tokens\n";
        return MS::kFailure;
    }

    tokens >> currentToken;

    if (currentToken.compare("Time:") != 0) {
        std::cerr << "Error in file content with tokens\n";
        return MS::kFailure;
    }
    
    tokens >> currentToken;

    float timeFrame = std::stof(currentToken);

    float time = 0;

    nodeQueue.clear();

    for (int i = 0; i < nbFrames; i++) {
        for (Node* root : rootVect) {
            nodeQueue.push_back(root);
            while (!nodeQueue.empty()) {
                Node* node = nodeQueue.back();
                nodeQueue.pop_back();
                bool state = readAnimNode(*node, tokens, time);
                if (!state) {
                    return MS::kFailure;
                }
                for (int childIndex = node->children.size() - 1; childIndex >= 0; i-- ) {
                    Node* child = node->children[childIndex];
                    nodeQueue.push_back(child);
                }
            }
        }
        time += timeFrame;
    }

    inputfile.close();

    //Create BVH
    for (Node* root : rootVect) {
        root->mayaCreate();
    }

    return rval;
}


bool BvhTranslator::readNode(Node& node, std::istringstream& tokens) {
    tokens >> node.name;
    std::string currentToken;
    tokens >> currentToken;
    if (currentToken.compare("{") != 0) {
        MGlobal::displayInfo("Ici !!!");
        std::cerr << "Error in file content with tokens\n";
        return false;
    }
    tokens >> currentToken;
    if (currentToken.compare("OFFSET") != 0) {
        MGlobal::displayInfo("La");
        std::cerr << "Error in file content with tokens\n";
        return false;
    }
    for (int i = 0; i < 3; i++) {
        tokens >> currentToken;
        node.offset[i] = std::stof(currentToken);
    }
    tokens >> currentToken;
    if (currentToken.compare("CHANNELS") != 0) {
        MGlobal::displayInfo("Ou bien ici");
        std::cerr << "Error in file content with tokens\n";
        return false;
    }
    tokens >> currentToken;
    int nbChannels = std::stoi(currentToken);
    for (int i = 0; i < nbChannels; i++) {
        tokens >> currentToken;
        node.channels.push_back(currentToken);
    }
    return true;

}

bool BvhTranslator::readAnimNode(Node& node, std::istringstream& tokens, float currentTime) {

    if (node.channels.empty()) {
        return true;
    }
    int nbChannels = node.channels.size();


    return true;
}

// Whenever Maya needs to know the preferred extension of this file format,
// it calls this method. For example, if the user tries to save a file called
// "test" using the Save As dialog, Maya will call this method and actually
// save it as "test.bvh". Note that the period should *not* be included in
// the extension.
MString BvhTranslator::defaultExtension () const
{
    return "bvh";
}


//This method is pretty simple, maya will call this function
//to make sure it is really a file from our translator.
//To make sure, we have a little magic number and we verify against it.
MPxFileTranslator::MFileKind BvhTranslator::identifyFile (
                                        const MFileObject& fileName,
                                        const char* buffer,
                                        short size) const
{
    // Check the buffer for the "BVH" magic number, the
    // string "<BVH>\n"

    return kIsMyFileType;
}

MStatus initializePlugin( MObject obj )
{
    MStatus   status;
    MFnPlugin plugin( obj, PLUGIN_COMPANY, "3.0", "Any");

    // Register the translator with the system
    // The last boolean in this method is very important.
    // It should be set to true if the reader method in the derived class
    // intends to issue MEL commands via the MGlobal::executeCommand 
    // method.  Setting this to true will slow down the creation of
    // new objects, but allows MEL commands other than those that are
    // part of the Maya Ascii file format to function correctly.
    status =  plugin.registerFileTranslator( "Bvh",
                                        "bvhTranslator.rgb",
                                        BvhTranslator::creator);
    if (!status) 
    {
        status.perror("registerFileTranslator");
        return status;
    }

    return status;
}

MStatus uninitializePlugin( MObject obj )
{
    MStatus   status;
    MFnPlugin plugin( obj );

    status =  plugin.deregisterFileTranslator( "Bvh" );
    if (!status)
    {
        status.perror("deregisterFileTranslator");
        return status;
    }

    return status;
}

