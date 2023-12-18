
import maya.cmds as cmds

correspondance = {
    "Xposition" : "translateX",
    "Yposition" : "translateY",
    "Zposition" : "translateZ",
    "Xrotation" : "rotateX",
    "Yrotation" : "rotateY",
    "Zrotation" : "rotateZ",
}

class Node:

    def __init__(self, name, offset, channels, parent=None, children=None):
        if children == None: children = []
        self.name = name
        self.offset = offset
        self.channels = channels
        self.channelsValues = []
        self.parent = parent
        self.children = children
    
    def mayaCreate(self):
        args = {}
        args["name"] = self.name
        args["position"] = self.offset
        args["relative"] = True
        cmds.joint(**args)

        for i in range(len(self.channelsValues)):
            for j in range(len(self.channels)):
                args = {}
                args["attribute"] = correspondance[self.channels[j]]
                args["value"] = self.channelsValues[i][1+j]
                args["time"] = self.channelsValues[i][0]
                cmds.setKeyframe(self.name, **args)

        for child in self.children:
            cmds.select(self.name)
            child.mayaCreate()

    def __str__(self):
        return f"""Node : (name : {self.name}, offset : {self.offset}, channels : {self.channels}, joint : {self.children})"""


def readNode(tokens, i, keyWord):
    # t = tokens[i]
    # if t != keyWord:
    #     raise ValueError(f"\"{keyWord}\" token not found")

    name = tokens[i + 1]

    accolade = tokens[i + 2]
    if accolade != "{":
        raise ValueError("\"{\" token not found")

    offset = tokens[i + 3]
    if offset != "OFFSET":
        raise ValueError("\"OFFSET\" token not found")

    offsetValues = [float(tokens[i+4+j]) for j in range(3)]

    
    channels = tokens[i + 7]
    if channels != "CHANNELS":
        raise ValueError("\"CHANNELS\" token not found")

    channelsLen = int(tokens[i + 8])

    channelsName = [tokens[i+9+j] for j in range(channelsLen)]

    node = Node(name, offsetValues, channelsName)

    i += 9 + channelsLen

    return i, node

def readAnimNode(tokens, i, node, time):
    if not(node.channels):
        return i
    nbChannels = len(node.channels)
    keyFrame = [time]
    for _ in range(nbChannels):
        keyFrame.append(float(tokens[i]))
        i += 1
    node.channelsValues.append(keyFrame)
    return i

def readBVH(file):
    with open(file, 'r') as fch:
        content = fch.read()

        tokens = content.split()

    i = 0
    t = tokens[0]
    if t != "HIERARCHY":
        raise ValueError("\"HIERARCHY\" token not found")
    i += 1

    nextKeyword = tokens[i]

    rootList = []
    nodeQueue = []

    while nextKeyword == "ROOT":
        i, node = readNode(tokens, i, "ROOT")
        rootList.append(node)
        nodeQueue.append(node)

        while nodeQueue != []:
            t = tokens[i]
            if t == "JOINT":
                i, node = readNode(tokens, i, "JOINT")
                nodeQueue[-1].children.append(node)
                node.parent = nodeQueue[-1]
                nodeQueue.append(node)
            elif t == "End":
                name = tokens[i+1]
                t = tokens[i+2]
                if t != "{":
                    raise ValueError("\"{\" token not found")

                offset = tokens[i + 3]
                if offset != "OFFSET":
                    raise ValueError("\"OFFSET\" token not found")

                offsetValues = [float(tokens[i+4+j]) for j in range(3)]

                node = Node(name, offsetValues, None)

                nodeQueue[-1].children.append(node)

                t = tokens[i+7]
                if t != "}":
                    raise ValueError("\"}\" token not found")

                i += 8

            elif t == "}":
                nodeQueue.pop()
                i += 1
            else:
                raise ValueError("Missing valid token")

        nextKeyword = tokens[i]

    t = tokens[i]
    if t != "MOTION":
        raise ValueError("\"MOTION\" token not found")

    t = tokens[i+1]
    if t != "Frames:":
        raise ValueError("\"Frames:\" token not found")

    nbFrames = int(tokens[i+2])
    
    t = tokens[i+3]
    tnext = tokens[i+4]
    if t != "Frame:" and tnext != "Time:":
        raise ValueError("\"Frames:\" token not found")

    frameInterval = float(tokens[i+5])
    time = 0

    i += 6

    for _ in range(nbFrames):
        k = 0
        while k < len(rootList):
            node = rootList[k]
            stack = [node]
            while len(stack) != 0:
                node = stack.pop()
                i = readAnimNode(tokens, i, node, time)
                for child_index in range(len(node.children)-1, -1, -1):
                    child = node.children[child_index]
                    stack.append(child)
            k += 1
        time += frameInterval
    
    if i < len(tokens):
        raise IndexError("The file contains more values than expected")

    return rootList

def mayaCreateNodes(rootList):
    cmds.select(clear=True)
    for root in rootList:
        root.mayaCreate()
    cmds.select(clear=True)

# rootList = readBVH("C:/Users/felix/Documents/3A/Maya/run.bvh")
rootList = readBVH("C:/Users/felix/Documents/3A/Maya/cmuconvert-daz-86-94/87/87_03.bvh")

mayaCreateNodes(rootList)
