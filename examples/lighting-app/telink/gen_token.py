import json


def find_json(json_path):
    """
    Find token.json in given path.

    Args:
        json_path (string): The file path of token.json.

    Returns:
        json_data (structure): The json data structure.
    """
    try:
        with open(json_path, 'r') as f:
            json_data = json.load(f)

    except FileNotFoundError:
        print(json_path + " is not find, abort.")
        exit(1)

    except json.JSONDecodeError:
        print(json_path + " parse error, abort.")
        exit(1)

    return json_data


def get_json_data(json_data):
    """
    Get the json data from token.json.

    Args:
        json_data (structure): The data structure of json.

    Returns:
        [ start_addr, sizeByte, write_data ]: The array of json data.
    """
    startAddr = json_data.get("offset")
    sizeByte = json_data.get("sizeB")
    writeData = json_data.get("data")

    return [ startAddr, sizeByte, writeData ]


def intervals_intersect(interval1, interval2):
    if interval1[1] <= interval2[0] or interval1[0] >= interval2[1]:
        return False  # no intersection return false
    else:
        return True
 

def find_intersection(interval1, interval2):
    if not intervals_intersect(interval1, interval2):
        return []
    
    start = max(interval1[0], interval2[0])
    end = min(interval1[1], interval2[1])

    return [hex(start), hex(end)]


def two_find_intersection(intervalArry, interval):
    for inter in intervalArry:
        intersection = find_intersection(inter, interval)
        if len(intersection) != 0:  # has intersection
            print("Intersection: ", end="\n")
            print(intersection, end="\n")
            return -1
    return 0


def curOffsetAddrCheck(preOffset, preSizeB, curOffset, curSizeB, intervalArry, token_bin_size):
    preOffset_INT = int(preOffset, 16)
    curOffset_INT = int(curOffset, 16)

    """
    # # Intersection verification
    # intersection = find_intersection(
    #     [preOffset_INT + 0, preOffset_INT + preSizeB], 
    #     [curOffset_INT + 0, curOffset_INT + curSizeB]
    # )
    # if intersection != None:
    #     print("preOffset: " + str(preOffset) + ",preSizeB: " + str(preSizeB), end="\n")
    #     print("curOffset: " + str(curOffset) + ",curSizeB: " + str(curSizeB), end="\n")

    #     print("\nIntersection: ", end="\n")
    #     print(intersection, end="\n")

    #     print("\nThe current write offset address is invalid,", end="\n")
    #     print("there is an intersection between the current offset and the previous offset,", end="\n")
    #     print("abort!", end="\n")

    #     exit(1)

    # # Upward interval verification
    # if curOffset_INT < (preOffset_INT + preSizeB):
    #     print("preOffset: " + str(preOffset) + ",preSizeB: " + str(preSizeB), end="\n")
    #     print("curOffset: " + str(curOffset) + ",curSizeB: " + str(curSizeB), end="\n")

    #     print("\nThe current write offset address is invalid,", end="\n")
    #     print("there is a risk of overwriting the previous write interval,", end="\n")
    #     print("abort!", end="\n")

    #     exit(1)
    """

    # two way interval verification
    result = two_find_intersection(
        intervalArry, 
        [curOffset_INT + 0, curOffset_INT + curSizeB]
    )
    if result == (-1):
        print("\ncurOffset: " + str(curOffset) + ",curSizeB: " + str(curSizeB), end="\n")

        print("\nThe current write offset address is invalid,", end="\n")
        print("there is a risk of overwriting the previous write interval,", end="\n")
        print("abort!", end="\n")

        exit(1)

    # Is or not exceed token.bin verification
    if (curOffset_INT + curSizeB) >= token_bin_size:
        print("curOffset: " + curOffset + ",curSizeB: " + curSizeB, end="\n")

        print("The current write offset address is invalid,", end="\n")
        print("exceed the write range of the token.bin,", end="\n")
        print("abort!", end="\n")

        exit(1)


def offsetNewSet(preOffset, preSizeB, curOffset):
    _TMP_ = curOffset.split("0x")

    if len(_TMP_) == 1:  # use pre offset + pre sizeB to calculate cur offset
        curOffset = hex(
            int(preOffset, 16) + preSizeB
        )
    
    return curOffset


def covertData(data):
    _TMP_ = data.split("0x")
    
    if len(_TMP_) == 1:
        return [format(ord(char), '02x') for char in _TMP_[0]]
    else:
        return [Str.strip().replace(',', '') for Str in _TMP_[1:]]


def adjustWriteData(sizeB, data, offset, baseVAL="ff"):
    dataLen = len(data)

    if sizeB == dataLen:
        return data
    elif sizeB > dataLen:
        return (data + ([baseVAL] * (sizeB - dataLen))) 
    elif sizeB < dataLen:
        print("offset: " + str(offset) + ", sizeB: " + str(sizeB) + ", dataLen: " + str(dataLen), end="\n")
        print("sizeB < len(data), illegal operation, abort!", end="\n")
        exit(1)
        # return data[0:(dataLen - sizeB)]


def gen_token_bin(token_bin_path, offset, sizeB, data):
    """
    Generate token.bin according to the data of token.json.

    Args:
        token_bin_path (string): The path of token.bin for new generate.
        addr (int): Location of written data.
        data (string): The data to be written.
    """
    try:
        with open(token_bin_path, 'r+b') as f:
            f.seek(int(offset, 16))

            for val in data:
                f.write(bytes.fromhex(val))

    except IOError:
        print("An error occurred while open or write to the bin, abort.")
        exit(1)


def main():
    """
    Set the basic working directory in 
    'examples/lighting-app/telink/...' 
    for gen_token.py
    """

    token_json_path = "./token.json"
    token_bin_path = "./token.bin"

    token_bin_size = 4096  # 4KB

    # Create a bin with all values of '0xff'
    with open(token_bin_path, 'wb+') as f: 
        f.write(bytes([0xff] * token_bin_size)) 

    # Find the token.json.
    _JSON_DATA_ = find_json(
        token_json_path
    )

    preOffset = "null"
    preSizeB = -1

    intervalArry = []

    for json_data in _JSON_DATA_:
        # Get the json data.
        offset, sizeB, data = get_json_data(json_data)

        # Init preOffset and preSizeB.
        if (preOffset == "null") or (preSizeB == -1):
            preOffset = offset
            preSizeB = sizeB

        # Get a new offset addr base on rule.
        newOffset = offsetNewSet(
            preOffset, 
            preSizeB, 
            offset
        )
        # print(newOffset, end = "\t")
        # print(sizeB, end = "\n")

        # Check if the current write offset address is valid.
        curOffsetAddrCheck(
            preOffset, 
            preSizeB, 
            newOffset,
            sizeB,
            intervalArry,
            token_bin_size
        )

        # Covert input data and get the new write data base on rule.
        covertArray = covertData(data)
        newData = adjustWriteData(sizeB, covertArray, newOffset, "ff")  # default supplement 0xff.
        # print(covertArray, end = "\n")
        # print(newData, end = "\n\n")

        # Generate token bin.
        gen_token_bin(
            token_bin_path, 
            newOffset, 
            sizeB, 
            newData
        )

        # Update preOffset and preSizeB.
        preOffset = newOffset
        preSizeB = sizeB
        # Use to two way find intersection.
        intervalArry.append([
            int(preOffset, 16) + 0, 
            int(preOffset, 16) + preSizeB
        ])
        # print(intervalArry, end="\n\n")
    
    print("Generate successfully for the token.bin.")


if __name__ == '__main__':
    main()
