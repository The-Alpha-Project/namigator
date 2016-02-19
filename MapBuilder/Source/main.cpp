#include "MeshBuilder.hpp"
#include "Worker.hpp"

#include "parser/Include/Adt/Adt.hpp"
#include "parser/Include/Wmo/WmoInstance.hpp"

#include "utility/Include/Directory.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char *argv[])
{
    std::string dataPath, map, outputPath;
    int adtX, adtY, jobs;

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("data,d", boost::program_options::value<std::string>(&dataPath)->default_value(".")->required(),           "data folder")
        ("map,m", boost::program_options::value<std::string>(&map)->required(),                                     "map")
        ("output,o", boost::program_options::value<std::string>(&outputPath)->default_value(".\\Maps")->required(), "output path")
        ("adtX,x", boost::program_options::value<int>(&adtX),                                                       "adt x")
        ("adtY,y", boost::program_options::value<int>(&adtY),                                                       "adt y")
        ("jobs,j", boost::program_options::value<int>(&jobs)->default_value(8)->required(),                         "build jobs")
        ("help,h",                                                                                                  "display help message");

    boost::program_options::variables_map vm;

    try
    {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }
    }
    catch (boost::program_options::error const &e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << desc << std::endl;

        return EXIT_FAILURE;
    }

    utility::Directory::Create(outputPath);
    utility::Directory::Create(outputPath + "\\BVH");
    utility::Directory::Create(outputPath + "\\Nav");

    MeshBuilder meshBuilder(dataPath, outputPath, map);

    if (vm.count("adtX") && vm.count("adtY"))
    {
        if (meshBuilder.IsGlobalWMO())
        {
            std::cerr << "ERROR: Specified Map has no ADTs" << std::endl;
            std::cerr << desc << std::endl;

            return EXIT_FAILURE;
        }

        {
            meshBuilder.SingleAdt(adtX, adtY);
        
            std::cout << "Building " << map << " (" << adtX << ", " << adtY << ")..." << std::endl;
            Worker worker(&meshBuilder);
        }

        std::cout << "Finished.";
        return EXIT_SUCCESS;
    }

    // either both, or neither should be specified
    if (vm.count("adtX") || vm.count("adtY"))
    {
        std::cerr << "ERROR: Must specify ADT X and Y" << std::endl;
        std::cerr << desc << std::endl;

        return EXIT_FAILURE;
    }

    // if the Map is a single wmo, we have no use for multiple threads
    if (meshBuilder.IsGlobalWMO())
    {
        std::cout << "Building global WMO for " << map << "..." << std::endl;
        {
            Worker worker(&meshBuilder, true);
        }

        meshBuilder.SaveMap();
        return EXIT_SUCCESS;
    }

    // once we reach here it is the usual case of generating an entire Map
    std::vector<std::unique_ptr<Worker>> workers(jobs);

    for (auto &worker : workers)
        worker.reset(new Worker(&meshBuilder));

    auto const start = time(NULL);

    for (;;)
    {
        bool done = true;
        for (auto &worker : workers)
            if (worker->IsRunning())
            {
                done = false;
                break;
            }

        if (done)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    meshBuilder.SaveMap();

    auto const stop = time(NULL);

    auto const runTime = stop - start;
    auto const adts = meshBuilder.AdtCount();
    auto const secPerAdt = runTime / adts;
    
    std::cout << "Finished " << map << " (" << adts << " ADTs) in " << runTime << " seconds." << std::endl;

    return EXIT_SUCCESS;
}