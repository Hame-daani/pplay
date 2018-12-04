//
// Created by cpasjuste on 04/12/18.
//

#ifndef PPLAY_MEDIAFILE_H
#define PPLAY_MEDIAFILE_H

#include "cross2d/skeleton/io.h"
#include "media.h"

class MediaFile : public c2d::Io::File {

public:

    MediaFile() = default;

    MediaFile(const c2d::Io::File &file, const Media &media = {}) {
        name = file.name;
        path = file.path;
        type = file.type;
        size = file.size;
        color = file.color;
        this->media = media;
    }

    Media media;
};

#endif //PPLAY_MEDIAFILE_H
